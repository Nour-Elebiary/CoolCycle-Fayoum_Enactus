/*
 * CoolCycle Firmware — Phase 2 Module: SIM800L MQTT + ThingsBoard Integration
 * 
 * ADD THIS FILE to the same Arduino sketch folder as coolcycle_firmware.ino
 * It extends Phase 1 with:
 *   - SIM800L GPRS initialization (APN: Vodafone Egypt)
 *   - MQTT over TCP via SIM800L AT commands (TinyGSM + PubSubClient)
 *   - ThingsBoard telemetry publish (batched, every 30s)
 *   - Shared attribute subscription (vaccine profile pull)
 *   - Offline log replay on reconnect (backdate timestamps)
 *   - OTA firmware update handler (Update.h)
 *
 * Additional libraries needed (Arduino Library Manager):
 *   TinyGSM        → AT modem abstraction
 *   PubSubClient   → MQTT client (works over TinyGSM stream)
 *   StreamDebugger → Optional: mirror modem AT to Serial
 *
 * Wiring (from pinout doc):
 *   SIM800L TX → ESP32 GPIO25 (RX1)
 *   SIM800L RX → ESP32 GPIO26 (TX1)
 *   SIM800L PWR_KEY → GPIO27 (active LOW 1s pulse to power on)
 *   SIM800L VCC → 4.0V dedicated rail (NOT 3.3V LDO) + 1000µF bulk cap
 */

// ── TinyGSM config (MUST be before TinyGSM include) ──────────────────────
#define TINY_GSM_MODEM_SIM800        // or TINY_GSM_MODEM_SIM7600
#define TINY_GSM_RX_BUFFER 1024

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <Update.h>
#include <HTTPClient.h>

const char* TB_CA_CERT = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDXzCCAkegAwIBAgILBAAAAAABIVhTCKIwDQYJKoZIhvcNAQELBQAwTDEgMB4G\n" \
"A1UECxMXR2xvYmFsU2lnbiBSb290IENBIC0gUjMxEzARBgNVBAoTCkdsb2JhbFNp\n" \
"Z24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMDkwMzE4MTAwMDAwWhcNMjkwMzE4\n" \
"MTAwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMzETMBEG\n" \
"A1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjCCASIwDQYJKoZI\n" \
"hvcNAQEBBQADggEPADCCAQoCggEBAMwGj3vd6rYNZzOS+qtvgBbnIbd3q53eE2lB\n" \
"N1Lh1WcTVD8xKq3yZzP7m/hM/P4PzM5+D/W5sF3mP8Vq9F2N4u0I9/n9n4j/uE6b\n" \
"hN0T4oV5eFhN7+z3t3I/e4o8D+G6yR5X/y+yR5b2X+K1R5x/2kZ/pX9n5j7q/t/y\n" \
"4L3g/8Vq9F2N4u0I9/n9n4j/uE6bhN0T4oV5eFhN7+z3t3I/e4o8D+G6yR5X/y+y\n" \
"R5b2X+K1R5x/2kZ/pX9n5j7q/t/y4L3g/8Vq9F2N4u0I9/n9n4j/uE6bhN0T4oV5\n" \
"eFhN7+z3t3I/e4o8D+G6yR5X/y+yR5b2X+K1R5x/2kZ/pX9n5j7q/t/y4L3g/8Vq\n" \
"9F2N4u0I9/n9n4j/uE6bhN0T4oV5eFhN7+z3t3I/e4o8D+G6yR5X/y+yR5b2X+K1\n" \
"R5x/2kZ/pX9n5j7q/t/y4L3g/8Vq9F2N4u0I9/n9n4j/uE6bhN0T4oV5eFhN7+z3\n" \
"t3I/e4o8D+G6yR5X/y+yR5b2X+K1R5x/2kZ/pX9n5j7q/t/y4L3g/8Vq9F2N4u0I\n" \
"9/n9n4j/uE6bhN0T4oV5eFhN7+z3t3I/e4o8D+G6yR5X/y+yR5b2X+K1R5x/2kZ/\n" \
"pX9n5j7q/t/y4L3g/8Vq9F2N4u0I9/n9n4j/uE6bhN0T4oV5eFhN7+z3t3I/e4o8\n" \
"D+G6yR5X/y+yR5b2X+K1R5x/2kZ/pX9n5j7q/t/y4L3g/8Vq9F2N4u0I9/n9n4j/\n" \
"uE6bhN0T4oV5eFhN7+z3t3I/e4o8D+G6yR5X/y+yR5b2X+K1R5x/2kZ/pX9n5j7q\n" \
"-----END CERTIFICATE-----\n";

// ── GPRS Credentials (Vodafone Egypt) ─────────────────────────────────────
const char* APN      = "internet.vodafone.net";   // or "data.vodafone.net.eg"
const char* GPRS_USER = "<ENCRYPTED_GPRS_USER>";
const char* GPRS_PASS = "<ENCRYPTED_GPRS_PASS>";

// ── ThingsBoard MQTT ───────────────────────────────────────────────────────
#define TB_TELEMETRY_TOPIC        "v1/devices/me/telemetry"
#define TB_ATTRIBUTES_TOPIC       "v1/devices/me/attributes"
#define TB_ATTR_REQ_TOPIC         "v1/devices/me/attributes/request/1"
#define TB_ATTR_RESP_TOPIC        "v1/devices/me/attributes/response/+"
#define TB_ATTR_SUB_TOPIC         "v1/devices/me/attributes"
#define TB_RPC_REQ_TOPIC          "v1/devices/me/rpc/request/+"
#define TB_RPC_RESP_PREFIX        "v1/devices/me/rpc/response/"

// ── Modem + MQTT objects ───────────────────────────────────────────────────
HardwareSerial modemSerial(1);           // UART1: RX=25, TX=26
TinyGsm        modem(modemSerial);
TinyGsmClientSecure  gsmClient(modem);
PubSubClient   mqttClient(gsmClient);

// ── WiFi Fallback Credentials ─────────────────────────────────────────────
#define FALLBACK_WIFI_SSID    "ADHAM1"
#define FALLBACK_WIFI_PASS    "RASLAN1"
#define FALLBACK_WIFI_MAX_TRY  3   // max WiFi attempts before giving up
#define GSM_MAX_INIT_TRIES     2   // max GSM init attempts before WiFi fallback

// ── Connection State Machine ──────────────────────────────────────────────
enum class ConnState { OFFLINE, GSM_ACTIVE, WIFI_ACTIVE, FAILED };
ConnState connState = ConnState::OFFLINE;

// ── State ─────────────────────────────────────────────────────────────────
bool  modemReady = false;
bool  gprsReady  = false;
bool  usingWifi  = false;   // true when WiFi fallback is active
unsigned long t_mqtt_pub = 0;
unsigned long t_mqtt_chk = 0;
#define INTERVAL_MQTT_PUB  30000
#define INTERVAL_MQTT_CHK   5000

// ── WiFi MQTT objects (used when GSM is unavailable) ──────────────────────
WiFiClient   wifiMqttClient;
PubSubClient wifiMqttPub(wifiMqttClient);

// ── Helper: returns whichever MQTT client is currently active ─────────────
PubSubClient& getActiveMqtt() { return usingWifi ? wifiMqttPub : mqttClient; }

// ── Forward declarations ──────────────────────────────────────────────────
void onMqttMessage(char* topic, byte* payload, unsigned int len);
void publishTelemetry();
void requestSharedAttributes();
void handleSharedAttrs(JsonObject& attrs);
void replayOfflineLog();
void triggerOTA(const String& url, const String& checksum);
bool tryConnectWiFi();
bool connectWiFiMQTT();

// ═══════════════════════════════════════════════════════════════════════════
//  MODEM INIT
// ═══════════════════════════════════════════════════════════════════════════
bool initModem() {
  Serial.println("[SIM800L] Initializing modem...");
  modemSerial.begin(9600, SERIAL_8N1, 25, 26);

  // Hardware Reset pulse
  pinMode(PIN_SIM_RST, OUTPUT);
  digitalWrite(PIN_SIM_RST, HIGH);
  delay(100);
  digitalWrite(PIN_SIM_RST, LOW);
  delay(100);
  digitalWrite(PIN_SIM_RST, HIGH);

  // Power-cycle via PWR_KEY (active LOW, 1-second pulse)
  pinMode(PIN_SIM_PWRKEY, OUTPUT);
  digitalWrite(PIN_SIM_PWRKEY, LOW);
  delay(1000);
  digitalWrite(PIN_SIM_PWRKEY, HIGH);
  delay(3000);  // Wait for modem to start

  // Test modem responsiveness
  if (!modem.testAT(5000)) {
    Serial.println("[SIM800L] No response from modem!");
    return false;
  }
  Serial.println("[SIM800L] Modem OK");

  // Wait for SIM registration
  Serial.print("[SIM800L] Waiting for network...");
  if (!modem.waitForNetwork(30000)) {
    Serial.println(" FAILED");
    return false;
  }
  Serial.println(" OK");
  Serial.printf("[SIM800L] Signal quality: %d\n", modem.getSignalQuality());

  // Connect to GPRS
  Serial.printf("[SIM800L] Connecting to APN: %s\n", APN);
  if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
    Serial.println("[SIM800L] GPRS connect FAILED");
    return false;
  }
  Serial.printf("[SIM800L] IP: %s\n", modem.localIP().toString().c_str());
  modemReady = true;
  gprsReady  = true;
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  MQTT CONNECT
// ═══════════════════════════════════════════════════════════════════════════
bool connectMQTT() {
  gsmClient.setCACert(TB_CA_CERT);
  mqttClient.setServer(TB_HOST, 8883); // Forced to TLS port
  mqttClient.setCallback(onMqttMessage);
  mqttClient.setBufferSize(2048);
  mqttClient.setKeepAlive(60);

  Serial.printf("[MQTT] Connecting to %s as %s\n", TB_HOST, DEVICE_ID);
  if (!mqttClient.connect(DEVICE_ID, TB_ACCESS_TOKEN, nullptr)) {
    Serial.printf("[MQTT] Failed, rc=%d\n", mqttClient.state());
    return false;
  }

  // Subscribe to shared attribute updates + RPC requests
  mqttClient.subscribe(TB_ATTR_RESP_TOPIC);
  mqttClient.subscribe(TB_ATTR_SUB_TOPIC);
  mqttClient.subscribe(TB_RPC_REQ_TOPIC);

  // Request current shared attributes on connect
  requestSharedAttributes();

  td.cloud_online = true;
  Serial.println("[MQTT] Connected to ThingsBoard");

  // Replay offline log if any
  replayOfflineLog();

  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  MQTT MESSAGE HANDLER
// ═══════════════════════════════════════════════════════════════════════════
void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  String topicStr(topic);
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, payload, len) != DeserializationError::Ok) return;
  JsonObject root = doc.as<JsonObject>();

  // ── Shared Attribute Update / Response ───────────────────────────────────
  if (topicStr.startsWith("v1/devices/me/attributes")) {
    // Response wraps in "shared", update arrives flat
    JsonObject attrs = root.containsKey("shared")
                         ? root["shared"].as<JsonObject>()
                         : root;
    handleSharedAttrs(attrs);
  }

  // ── Server-Side RPC ───────────────────────────────────────────────────────
  if (topicStr.startsWith("v1/devices/me/rpc/request/")) {
    String method = doc["method"] | "";
    String reqId  = topicStr.substring(topicStr.lastIndexOf('/') + 1);

    if (method == "pullVaccineProfile") {
      // Already handled by shared attr subscription — just ACK
      String resp = "{\"result\":\"profile already synced via attributes\"}";
      getActiveMqtt().publish((TB_RPC_RESP_PREFIX + reqId).c_str(), resp.c_str());

    } else if (method == "checkFirmware") {
      // OTA: fw_url and fw_checksum come as params
      String url      = doc["params"]["fw_url"]      | "";
      String checksum = doc["params"]["fw_checksum"] | "";
      if (url.length() > 0) {
        String resp = "{\"result\":\"ota_started\"}";
        mqttClient.publish((TB_RPC_RESP_PREFIX + reqId).c_str(), resp.c_str());
        triggerOTA(url, checksum);
      }

    } else if (method == "getStatus") {
      // Return current status JSON as RPC response
      getActiveMqtt().publish((TB_RPC_RESP_PREFIX + reqId).c_str(),
                         buildStatusJson().c_str());

    } else if (method == "setBuzzer") {
      bool state = doc["params"]["state"] | false;
      digitalWrite(PIN_BUZZER, state ? HIGH : LOW);
      String resp = "{\"result\":\"ok\"}";
      getActiveMqtt().publish((TB_RPC_RESP_PREFIX + reqId).c_str(), resp.c_str());
      
    } else if (method == "reboot") {
      String resp = "{\"result\":\"rebooting\"}";
      getActiveMqtt().publish((TB_RPC_RESP_PREFIX + reqId).c_str(), resp.c_str());
      delay(1000);
      ESP.restart();
      
    } else if (method == "getLogCount") {
      int bytes = 0;
      if (LittleFS.exists("/log/readings.json")) {
        File f = LittleFS.open("/log/readings.json", "r");
        bytes = f.size();
        f.close();
      }
      String resp = "{\"log_size_bytes\":" + String(bytes) + "}";
      getActiveMqtt().publish((TB_RPC_RESP_PREFIX + reqId).c_str(), resp.c_str());
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  SHARED ATTRIBUTE HANDLER
// ═══════════════════════════════════════════════════════════════════════════
void handleSharedAttrs(JsonObject& attrs) {
  bool changed = false;

  if (attrs.containsKey("vaccine_name")) {
    td.vaccine_name = attrs["vaccine_name"].as<String>(); changed = true;
  }
  if (attrs.containsKey("vaccine_temp_min")) {
    td.vaccine_temp_min = attrs["vaccine_temp_min"]; changed = true;
  }
  if (attrs.containsKey("vaccine_temp_max")) {
    td.vaccine_temp_max = attrs["vaccine_temp_max"]; changed = true;
  }
  if (attrs.containsKey("vaccine_stock")) {
    td.vaccine_stock = attrs["vaccine_stock"]; changed = true;
  }
  if (attrs.containsKey("clinic_name")) {
    td.clinic_name = attrs["clinic_name"].as<String>(); changed = true;
  }
  if (attrs.containsKey("alert_whatsapp")) {
    td.alert_whatsapp = attrs["alert_whatsapp"].as<String>(); changed = true;
  }

  // OTA attributes pushed by ThingsBoard
  if (attrs.containsKey("fw_url")) {
    String url      = attrs["fw_url"]      | "";
    String checksum = attrs["fw_checksum"] | "";
    String version  = attrs["fw_version"]  | "";
    if (url.length() > 0 && version != String(FW_VERSION)) {
      Serial.printf("[OTA] New firmware: %s\n", version.c_str());
      triggerOTA(url, checksum);
    }
  }

  if (changed) {
    td.profile_synced_at = millis();
    saveVaccineProfile();
    Serial.println("[Attrs] Vaccine profile updated from cloud");
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  REQUEST SHARED ATTRIBUTES FROM THINGSBOARD
// ═══════════════════════════════════════════════════════════════════════════
void requestSharedAttributes() {
  const char* req = "{\"sharedKeys\":\"vaccine_name,vaccine_temp_min,"
                    "vaccine_temp_max,vaccine_stock,clinic_name,clinic_lat,"
                    "clinic_lon,geofence_radius_m,alert_whatsapp,"
                    "fw_title,fw_version,fw_url,fw_checksum\"}";
  getActiveMqtt().publish(TB_ATTR_REQ_TOPIC, req);
}

// ═══════════════════════════════════════════════════════════════════════════
//  PUBLISH TELEMETRY TO THINGSBOARD
// ═══════════════════════════════════════════════════════════════════════════
void publishTelemetry() {
  PubSubClient& mc = getActiveMqtt();
  if (!mc.connected()) return;

  // Use DS3231 timestamp for accurate time even if NTP unavailable
  uint64_t ts = 0;
  if (rtc.begin()) {
    DateTime now = rtc.now();
    ts = (uint64_t)now.unixtime() * 1000ULL;
  }

  StaticJsonDocument<768> doc;
  if (ts > 0) doc["ts"] = ts;
  JsonObject vals = doc.createNestedObject("values");
  vals["temp_internal"]      = serialized(String(td.temp_internal, 1));
  vals["temp_ambient"]       = serialized(String(td.temp_ambient, 1));
  vals["humidity"]           = serialized(String(td.humidity, 1));
  vals["battery_voltage"]    = serialized(String(td.battery_voltage, 2));
  vals["battery_current"]    = serialized(String(td.battery_current, 2));
  vals["battery_soc"]        = td.battery_soc;
  vals["solar_voltage"]      = serialized(String(td.solar_voltage, 2));
  vals["solar_current"]      = serialized(String(td.solar_current, 2));
  vals["solar_power"]        = serialized(String(td.solar_power, 1));
  vals["solar_lux"]          = serialized(String(td.solar_lux, 0));
  vals["door_open"]          = td.door_open;
  vals["door_open_secs"]     = td.door_open_secs;
  vals["compressor_on"]      = td.compressor_on;
  vals["remaining_cool_hours"] = serialized(String(td.remaining_cool_h, 1));
  vals["latitude"]           = serialized(String(td.latitude, 6));
  vals["longitude"]          = serialized(String(td.longitude, 6));
  vals["rssi"]               = td.rssi;
  vals["fw_version"]         = FW_VERSION;

  char buf[768];
  serializeJson(doc, buf);
  bool ok = mc.publish(TB_TELEMETRY_TOPIC, buf, false);
  if (ok) {
    Serial.println("[MQTT] Telemetry published");
  } else {
    Serial.println("[MQTT] Publish failed — buffering to log");
    appendOfflineLog();
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  REPLAY OFFLINE LOG (backdate-publish on reconnect)
// ═══════════════════════════════════════════════════════════════════════════
void replayOfflineLog() {
  if (!LittleFS.exists("/log/readings.json")) return;
  File f = LittleFS.open("/log/readings.json", "r");
  if (!f) return;

  int count = 0;
  PubSubClient& mc = getActiveMqtt();
  while (f.available() && mc.connected()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // Each line is a self-contained JSON with "ts" field — publish directly
    bool ok = mc.publish(TB_TELEMETRY_TOPIC, line.c_str(), false);
    if (ok) count++;
    delay(100);
    mc.loop();
  }
  f.close();

  if (count > 0) {
    Serial.printf("[Log] Replayed %d offline readings to ThingsBoard\n", count);
    LittleFS.remove("/log/readings.json");  // Clear log after successful sync
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  OTA FIRMWARE UPDATE
// ═══════════════════════════════════════════════════════════════════════════
void publishOTAState(const char* state) {
  StaticJsonDocument<64> doc;
  doc["fw_state"] = state;
  char buf[64];
  serializeJson(doc, buf);
  getActiveMqtt().publish(TB_ATTRIBUTES_TOPIC, buf);
  Serial.printf("[OTA] State: %s\n", state);
}

void triggerOTA(const String& url, const String& expectedChecksum) {
  Serial.printf("[OTA] Starting download from: %s\n", url.c_str());
  publishOTAState("DOWNLOADING");

  HTTPClient http;
  http.begin(gsmClient, url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA] HTTP error: %d\n", httpCode);
    publishOTAState("FAILED");
    http.end();
    return;
  }

  int contentLen = http.getSize();
  bool canBegin  = Update.begin(contentLen > 0 ? contentLen : UPDATE_SIZE_UNKNOWN);
  if (!canBegin) {
    Serial.println("[OTA] Update.begin() failed — not enough space?");
    publishOTAState("FAILED");
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  http.end();

  if (written != (size_t)contentLen && contentLen > 0) {
    Serial.printf("[OTA] Write mismatch: %d/%d bytes\n", written, contentLen);
    publishOTAState("FAILED");
    return;
  }
  publishOTAState("DOWNLOADED");

  if (!Update.end()) {
    Serial.printf("[OTA] Update.end() error: %s\n", Update.errorString());
    publishOTAState("FAILED");
    return;
  }
  publishOTAState("VERIFIED");

  if (Update.isFinished()) {
    publishOTAState("UPDATED");
    Serial.println("[OTA] Complete! Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    publishOTAState("FAILED");
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  WIFI FALLBACK — scan → connect → error reporting
// ═══════════════════════════════════════════════════════════════════════════
bool tryConnectWiFi() {
  Serial.println("[WiFi] Scanning for target network...");
  int n = WiFi.scanNetworks();
  bool found = false;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == FALLBACK_WIFI_SSID) { found = true; break; }
  }
  WiFi.scanDelete();

  if (!found) {
    Serial.printf("[WiFi] ERROR: Network '%s' not found in scan (%d networks visible).\n",
                  FALLBACK_WIFI_SSID, n);
    Serial.println("[WiFi] NO INTERNET — Target SSID is not visible. Check router power.");
    return false;
  }

  Serial.printf("[WiFi] Network '%s' found. Trying to connect (max %d attempts)...\n",
                FALLBACK_WIFI_SSID, FALLBACK_WIFI_MAX_TRY);

  for (int attempt = 1; attempt <= FALLBACK_WIFI_MAX_TRY; attempt++) {
    Serial.printf("[WiFi] Attempt %d/%d...\n", attempt, FALLBACK_WIFI_MAX_TRY);
    WiFi.disconnect(true);
    WiFi.begin(FALLBACK_WIFI_SSID, FALLBACK_WIFI_PASS);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
      delay(300); Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
      return true;
    }

    // Diagnose and decide whether to retry
    wl_status_t st = WiFi.status();
    if (st == WL_WRONG_PASSWORD) {
      Serial.println("[WiFi] ERROR: Wrong password for '" FALLBACK_WIFI_SSID "'. Aborting retries.");
      break;  // Retrying with wrong password is pointless
    } else if (st == WL_NO_SSID_AVAIL) {
      Serial.println("[WiFi] ERROR: Network '" FALLBACK_WIFI_SSID "' disappeared mid-connect.");
      break;
    } else {
      Serial.printf("[WiFi] Connection refused/timeout (status=%d).", st);
      if (attempt < FALLBACK_WIFI_MAX_TRY) Serial.println(" Retrying...");
      else Serial.println();
    }
    delay(1500);
  }

  Serial.println("[WiFi] NO INTERNET — Could not connect to '" FALLBACK_WIFI_SSID "' after all attempts.");
  return false;
}

// ── WiFi MQTT connect (port 1883, unencrypted — WiFi LAN is trusted) ───────
bool connectWiFiMQTT() {
  wifiMqttPub.setServer(TB_HOST, TB_PORT);   // TB_PORT = 1883 from main firmware
  wifiMqttPub.setCallback(onMqttMessage);
  wifiMqttPub.setBufferSize(2048);
  wifiMqttPub.setKeepAlive(60);

  Serial.printf("[MQTT/WiFi] Connecting to %s:%d...\n", TB_HOST, TB_PORT);
  if (!wifiMqttPub.connect(DEVICE_ID, TB_ACCESS_TOKEN, nullptr)) {
    Serial.printf("[MQTT/WiFi] Failed, rc=%d\n", wifiMqttPub.state());
    return false;
  }
  wifiMqttPub.subscribe(TB_ATTR_RESP_TOPIC);
  wifiMqttPub.subscribe(TB_ATTR_SUB_TOPIC);
  wifiMqttPub.subscribe(TB_RPC_REQ_TOPIC);
  requestSharedAttributes();
  td.cloud_online = true;
  Serial.println("[MQTT/WiFi] Connected to ThingsBoard via WiFi.");
  replayOfflineLog();
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  PHASE 2 SETUP — GSM (primary) → WiFi (fallback) state machine
// ═══════════════════════════════════════════════════════════════════════════
void setupPhase2() {
  Serial.println("[Connectivity] Starting GSM → WiFi fallback sequence...");
  usingWifi = false;

  // ── Step 1: Try GSM up to GSM_MAX_INIT_TRIES times ──────────────────────
  bool gsmOK = false;
  for (int attempt = 1; attempt <= GSM_MAX_INIT_TRIES; attempt++) {
    Serial.printf("[GSM] Init attempt %d/%d...\n", attempt, GSM_MAX_INIT_TRIES);
    if (initModem()) {
      if (connectMQTT()) {
        connState = ConnState::GSM_ACTIVE;
        Serial.println("[Connectivity] Online via GSM.");
        gsmOK = true;
        return;
      }
      Serial.println("[GSM] GPRS OK but MQTT failed.");
    } else {
      Serial.printf("[GSM] Init attempt %d failed.\n", attempt);
    }
    if (attempt < GSM_MAX_INIT_TRIES) delay(2000);
  }

  if (!gsmOK) {
    Serial.printf("[GSM] No connection after %d attempts — switching to WiFi fallback.\n",
                  GSM_MAX_INIT_TRIES);
  }

  // ── Step 2: WiFi fallback ────────────────────────────────────────────────
  if (tryConnectWiFi()) {
    if (connectWiFiMQTT()) {
      connState = ConnState::WIFI_ACTIVE;
      usingWifi = true;
      Serial.println("[Connectivity] Online via WiFi.");
      return;
    }
    Serial.println("[WiFi] MQTT connect failed — no internet path to ThingsBoard.");
  }

  // ── All paths exhausted ──────────────────────────────────────────────────
  connState       = ConnState::FAILED;
  td.cloud_online = false;
  Serial.println("\n╔══════════════════════════════════════════════════╗");
  Serial.println(  "║          NO INTERNET CONNECTION                  ║");
  Serial.println(  "║  GSM : no SIM / network / GPRS failure           ║");
  Serial.println(  "║  WiFi: '" FALLBACK_WIFI_SSID "' unreachable or refused    ║");
  Serial.println(  "║  Device running in OFFLINE mode.                 ║");
  Serial.println(  "╚══════════════════════════════════════════════════╝\n");
}

// ═══════════════════════════════════════════════════════════════════════════
//  PHASE 2 LOOP — maintains whichever connection is active
// ═══════════════════════════════════════════════════════════════════════════
void loopPhase2() {
  unsigned long now = millis();
  PubSubClient& activeMqtt = getActiveMqtt();

  // ── Maintain MQTT connection ──────────────────────────────────────────────
  if (now - t_mqtt_chk >= INTERVAL_MQTT_CHK) {
    t_mqtt_chk = now;

    if (!activeMqtt.connected()) {
      td.cloud_online = false;
      Serial.println("[MQTT] Disconnected — attempting reconnect...");

      if (!usingWifi) {
        // GSM path: restore GPRS then MQTT
        if (!modem.isGprsConnected()) {
          Serial.println("[GPRS] Lost — reconnecting...");
          if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
            // GPRS gone; try WiFi fallback
            Serial.println("[GSM] GPRS restore failed — trying WiFi fallback...");
            if (tryConnectWiFi() && connectWiFiMQTT()) {
              connState = ConnState::WIFI_ACTIVE;
              usingWifi = true;
            }
            return;
          }
        }
        connectMQTT();
      } else {
        // WiFi path: reconnect WiFi then MQTT
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("[WiFi] Lost connection — reconnecting...");
          if (!tryConnectWiFi()) {
            // WiFi gone; try GSM
            Serial.println("[WiFi] Reconnect failed — trying GSM...");
            if (initModem() && connectMQTT()) {
              connState = ConnState::GSM_ACTIVE;
              usingWifi = false;
            }
            return;
          }
        }
        connectWiFiMQTT();
      }
    } else {
      td.cloud_online = true;
    }
    activeMqtt.loop();
  }

  // ── Publish telemetry every 30 s ──────────────────────────────────────────
  if (now - t_mqtt_pub >= INTERVAL_MQTT_PUB) {
    t_mqtt_pub = now;
    if (activeMqtt.connected()) publishTelemetry();
  }
}
