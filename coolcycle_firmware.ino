/*
 * CoolCycle Firmware v1.0 — Phase 1 (Local Sensing + Offline Dashboard)
 * ESP32-DevKit-V4 (WROOM-32D)
 *
 * Sensors: DS18B20, DHT22, INA219×2, DS3231, BH1750, Neo-6M GPS, Reed Switch
 * Local: AsyncWebServer @ 192.168.4.1 (Soft-AP) serving /api/status JSON
 * Storage: LittleFS — vaccine profile (/config/vaccine.json), logs (/log/)
 * Phase 2 additions: MQTT + SIM800L (stubs included)
 *
 * Libraries required (Arduino Library Manager):
 *   DallasTemperature, OneWire, DHT sensor library (Adafruit),
 *   Adafruit INA219, RTClib, BH1750, TinyGPSPlus,
 *   ESPAsyncWebServer, AsyncTCP, ArduinoJson, LittleFS (built-in ESP32)
 */
// ─────────────────────────────────────────────
//  DEBUG CONFIGURATION
// ─────────────────────────────────────────────
#define DEBUG_MODE 1  // Set to 0 to disable verbose serial logging for production

#if DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// ─────────────────────────────────────────────
//  INCLUDES
// ─────────────────────────────────────────────
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <WiFiManager.h>
#include <Preferences.h>

// Sensors
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <RTClib.h>
#include <BH1750.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// ─────────────────────────────────────────────
//  CONFIG — EDIT THESE
// ─────────────────────────────────────────────
const char* CLINIC_WIFI_SSID = "<ENCRYPTED_CLINIC_WIFI_SSID>";
const char* CLINIC_WIFI_PASS = "<ENCRYPTED_CLINIC_WIFI_PASS>";
const char* AP_SSID          = "CoolCycle-CC0001";
const char* AP_PASS          = "<ENCRYPTED_AP_PASS>";
const char* DEVICE_ID        = "CC-0001";
const char* FW_VERSION        = "1.0.0";

// ThingsBoard (Phase 2 — fill in after provisioning)
const char* TB_HOST          = "thingsboard.cloud";
const int   TB_PORT          = 1883;
const char* TB_ACCESS_TOKEN  = "<ENCRYPTED_TB_ACCESS_TOKEN>";

// ─────────────────────────────────────────────
//  GPIO PIN DEFINITIONS
// ─────────────────────────────────────────────
#define PIN_DS18B20     4     // DS18B20 one-wire data
#define PIN_DHT22       5     // DHT22 data
#define PIN_REED        34    // Reed switch (input-only, external 10kΩ pull-up)
#define PIN_BUZZER      25    // Active buzzer (via transistor)
#define PIN_LED_RED     26    // Red alarm LED
#define PIN_LED_GREEN   27    // Green status LED
#define PIN_LDR         35    // LDR analog (ADC1 CH7)
#define PIN_SIM_PWRKEY  23    // SIM800L power key
#define PIN_CONFIG_BTN  0     // Boot button for reset/alarms

// I²C on GPIO 21 (SDA) / 22 (SCL) — default Wire pins
// UART2 for SIM800L: RX=16, TX=17
// GPS on Serial1: RX=18, TX=19

// ─────────────────────────────────────────────
//  TIMING INTERVALS (ms)
// ─────────────────────────────────────────────
#define INTERVAL_SENSORS   5000
#define INTERVAL_POWER     1000
#define INTERVAL_GPS      10000
#define INTERVAL_DOOR       500
#define INTERVAL_ALARMS    2000
#define INTERVAL_WEB_JSON  5000
#define INTERVAL_LOG      60000   // Write to LittleFS every minute offline
#define INTERVAL_MQTT     30000   // Phase 2

// Battery capacity constants
#define BATTERY_CAPACITY_AH  50.0f   // 4S4P LiFePO4 nominal capacity
#define COMPRESSOR_MIN_AMPS   6.0f   // Below this = compressor OFF

// ─────────────────────────────────────────────
//  SENSOR OBJECTS
// ─────────────────────────────────────────────
OneWire           oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);

DHT               dht(PIN_DHT22, DHT22);

Adafruit_INA219   ina_battery(0x40);   // A0=GND,A1=GND
Adafruit_INA219   ina_solar(0x41);     // A0=VCC,A1=GND

RTC_DS3231        rtc;
BH1750            bh1750;

TinyGPSPlus       gps;
HardwareSerial    gpsSerial(1);        // UART1 for GPS

Preferences       prefs;
AsyncWebServer    server(80);

// ─────────────────────────────────────────────
//  TELEMETRY DATA STRUCT
// ─────────────────────────────────────────────
struct TelemetryData {
  float  temp_internal    = 0.0;
  float  temp_ambient     = 0.0;
  float  humidity         = 0.0;
  float  battery_voltage  = 0.0;
  float  battery_current  = 0.0;
  int    battery_soc      = 0;
  float  solar_voltage    = 0.0;
  float  solar_current    = 0.0;
  float  solar_power      = 0.0;
  float  solar_lux        = 0.0;
  bool   door_open        = false;
  int    door_open_secs   = 0;
  bool   compressor_on    = false;
  float  remaining_cool_h = 0.0;
  double latitude         = 0.0;
  double longitude        = 0.0;
  bool   gps_valid        = false;
  bool   cloud_online     = false;
  int    rssi             = 0;
  float  esp32_temp_c     = 0.0;
  int    charge_cycle_count = 0;

  // Vaccine profile (loaded from LittleFS)
  String vaccine_name       = "N/A";
  float  vaccine_temp_min   = 2.0;
  float  vaccine_temp_max   = 8.0;
  int    vaccine_stock      = 0;
  String clinic_name        = "CoolCycle Unit";
  String alert_whatsapp     = "";
  unsigned long profile_synced_at = 0;
} td;

float battery_charge_Ah = BATTERY_CAPACITY_AH;
float solar_history[15] = {0};
int   solar_history_idx = 0;
bool  solar_history_filled = false;

// ─────────────────────────────────────────────
//  TIMER VARIABLES
// ─────────────────────────────────────────────
unsigned long t_sensors  = 0;
unsigned long t_power    = 0;
unsigned long t_gps      = 0;
unsigned long t_door     = 0;
unsigned long t_alarms   = 0;
unsigned long t_log      = 0;

// Door timing
unsigned long door_opened_at = 0;

// ─────────────────────────────────────────────
//  LITTLEFS HELPERS
// ─────────────────────────────────────────────
void loadVaccineProfile() {
  if (!LittleFS.exists("/config/vaccine.json")) {
    DEBUG_PRINTLN("[LittleFS] No vaccine profile found, using defaults.");
    return;
  }
  File f = LittleFS.open("/config/vaccine.json", "r");
  if (!f) return;
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) { DEBUG_PRINTLN("[LittleFS] JSON parse error"); return; }

  td.vaccine_name     = doc["vaccine_name"]     | "OPV";
  td.vaccine_temp_min = doc["vaccine_temp_min"] | 2.0f;
  td.vaccine_temp_max = doc["vaccine_temp_max"] | 8.0f;
  td.vaccine_stock    = doc["vaccine_stock"]    | 0;
  td.clinic_name      = doc["clinic_name"]      | "CoolCycle Unit";
  td.alert_whatsapp   = doc["alert_whatsapp"]   | "";
  td.profile_synced_at= doc["profile_synced_at"]| (unsigned long)0;
  DEBUG_PRINTF("[LittleFS] Vaccine profile loaded: %s (%.1f-%.1f°C)\n",
    td.vaccine_name.c_str(), td.vaccine_temp_min, td.vaccine_temp_max);
}

void saveVaccineProfile() {
  LittleFS.mkdir("/config");
  File f = LittleFS.open("/config/vaccine.json", "w");
  if (!f) return;
  StaticJsonDocument<512> doc;
  doc["vaccine_name"]     = td.vaccine_name;
  doc["vaccine_temp_min"] = td.vaccine_temp_min;
  doc["vaccine_temp_max"] = td.vaccine_temp_max;
  doc["vaccine_stock"]    = td.vaccine_stock;
  doc["clinic_name"]      = td.clinic_name;
  doc["alert_whatsapp"]   = td.alert_whatsapp;
  doc["profile_synced_at"]= td.profile_synced_at;
  serializeJson(doc, f);
  f.close();
}

void loadLastGPS() {
  if (!LittleFS.exists("/config/lastgps.json")) return;
  File f = LittleFS.open("/config/lastgps.json", "r");
  if (!f) return;
  StaticJsonDocument<256> doc;
  if (!deserializeJson(doc, f)) {
    td.latitude  = doc["lat"];
    td.longitude = doc["lon"];
    td.gps_valid = true;
    DEBUG_PRINTF("[LittleFS] Loaded fallback GPS: %.6f, %.6f\n", td.latitude, td.longitude);
  }
  f.close();
}

void saveLastGPS() {
  LittleFS.mkdir("/config");
  File f = LittleFS.open("/config/lastgps.json", "w");
  if (!f) return;
  StaticJsonDocument<256> doc;
  doc["lat"] = td.latitude;
  doc["lon"] = td.longitude;
  serializeJson(doc, f);
  f.close();
}

void appendOfflineLog() {
  if (!rtc.begin()) return;
  LittleFS.mkdir("/log");
  File f = LittleFS.open("/log/readings.json", "a");
  if (!f) return;
  DateTime now = rtc.now();
  StaticJsonDocument<256> doc;
  doc["ts"]           = (unsigned long long)now.unixtime() * 1000ULL;
  doc["temp_internal"]= round(td.temp_internal * 10) / 10.0;
  doc["temp_ambient"] = round(td.temp_ambient * 10) / 10.0;
  doc["battery_soc"]  = td.battery_soc;
  doc["door_open"]    = td.door_open;
  doc["solar_power"]  = round(td.solar_power * 10) / 10.0;
  serializeJson(doc, f);
  f.write('\n');
  f.close();
}

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

// ─────────────────────────────────────────────
//  SOC ESTIMATION (Coulomb Counting)
// ─────────────────────────────────────────────
void updateBatterySOC() {
  float current_A = td.battery_current;
  float delta_Ah = current_A * (INTERVAL_POWER / 1000.0f) / 3600.0f;
  
  battery_charge_Ah += delta_Ah; // positive when charging, negative discharging
  
  // Recalibrate to 100% if solar voltage high and charging current tapers
  if (td.solar_voltage > 14.0f && current_A > 0 && current_A < 0.5f) {
    battery_charge_Ah = BATTERY_CAPACITY_AH;
  }
  
  battery_charge_Ah = constrain(battery_charge_Ah, 0.0f, BATTERY_CAPACITY_AH);
  td.battery_soc = (int)((battery_charge_Ah / BATTERY_CAPACITY_AH) * 100.0f);
  
  // Charge cycle tracking logic
  static bool was_low = false;
  if (td.battery_soc < 20) was_low = true;
  if (was_low && td.battery_soc > 80) {
    charge_cycle_count++;
    prefs.putInt("cycles", charge_cycle_count);
    was_low = false;
  }
}

// ─────────────────────────────────────────────
//  SOLAR ROLLING AVERAGE
// ─────────────────────────────────────────────
float getRollingSolarPower() {
  float sum = 0;
  int count = solar_history_filled ? 15 : solar_history_idx;
  if (count == 0) return 0;
  for (int i = 0; i < count; i++) sum += solar_history[i];
  return sum / count;
}

// ─────────────────────────────────────────────
//  REMAINING COOL TIME
// ─────────────────────────────────────────────
float calcRemainingCoolHours() {
  float capacity_wh    = (td.battery_soc / 100.0f) * BATTERY_CAPACITY_AH * td.battery_voltage;
  float compressor_w   = fabs(td.battery_current) * td.battery_voltage;
  float avg_solar      = getRollingSolarPower();
  
  // Use minimum assumed load floor of 5W to prevent false 99h readings
  float net_draw_w     = max(5.0f, compressor_w - avg_solar);
  float hours = capacity_wh / net_draw_w;
  return min(hours, 72.0f); // Cap at 72 hours max
}

// ─────────────────────────────────────────────
//  LOCAL ALARMS (buzzer + LED)
// ─────────────────────────────────────────────
void handleLocalAlarms() {
  bool critical = false;
  bool warning  = false;

  if (td.temp_internal < td.vaccine_temp_min || td.temp_internal > td.vaccine_temp_max)
    critical = true;
  if (td.battery_soc < 15)
    critical = true;
  if (td.door_open && td.door_open_secs > 60)
    warning = true;
  if (td.battery_soc < 30 && td.battery_soc >= 15)
    warning = true;

  if (critical) {
    digitalWrite(PIN_LED_RED,   HIGH);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_BUZZER,    HIGH);
  } else if (warning) {
    digitalWrite(PIN_LED_RED,   HIGH);
    digitalWrite(PIN_LED_GREEN, LOW);
    // Intermittent buzzer for warning (toggle every call ~2s)
    static bool buzzState = false;
    buzzState = !buzzState;
    digitalWrite(PIN_BUZZER, buzzState);
  } else {
    digitalWrite(PIN_LED_RED,   LOW);
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_BUZZER,    LOW);
  }
}

// ─────────────────────────────────────────────
//  BUILD STATUS JSON
// ─────────────────────────────────────────────
String buildStatusJson() {
  int log_count = 0;
  if (LittleFS.exists("/log/readings.json")) {
    File f = LittleFS.open("/log/readings.json", "r");
    while (f.available()) {
      if (f.read() == '\n') log_count++;
    }
    f.close();
  }

  StaticJsonDocument<768> doc;
  doc["device_id"]          = DEVICE_ID;
  doc["fw_version"]         = FW_VERSION;
  doc["clinic_name"]        = td.clinic_name;
  doc["temp_internal"]      = serialized(String(td.temp_internal, 1));
  doc["temp_ambient"]       = serialized(String(td.temp_ambient, 1));
  doc["humidity"]           = serialized(String(td.humidity, 1));
  doc["battery_voltage"]    = serialized(String(td.battery_voltage, 2));
  doc["battery_current"]    = serialized(String(td.battery_current, 2));
  doc["battery_soc"]        = td.battery_soc;
  doc["solar_voltage"]      = serialized(String(td.solar_voltage, 2));
  doc["solar_current"]      = serialized(String(td.solar_current, 2));
  doc["solar_power"]        = serialized(String(td.solar_power, 1));
  doc["solar_lux"]          = serialized(String(td.solar_lux, 0));
  doc["door_open"]          = td.door_open;
  doc["door_open_secs"]     = td.door_open_secs;
  doc["compressor_on"]      = td.compressor_on;
  doc["remaining_cool_hours"] = serialized(String(td.remaining_cool_h, 1));
  doc["latitude"]           = serialized(String(td.latitude, 6));
  doc["longitude"]          = serialized(String(td.longitude, 6));
  doc["gps_valid"]          = td.gps_valid;
  doc["cloud_online"]       = td.cloud_online;
  doc["offline_log_count"]  = log_count;
  doc["rssi"]               = td.rssi;
  doc["esp32_temp_c"]       = serialized(String(td.esp32_temp_c, 1));
  doc["charge_cycle_count"] = td.charge_cycle_count;
  doc["vaccine_name"]       = td.vaccine_name;
  doc["vaccine_temp_min"]   = td.vaccine_temp_min;
  doc["vaccine_temp_max"]   = td.vaccine_temp_max;
  doc["vaccine_stock"]      = td.vaccine_stock;
  doc["alert_whatsapp"]     = td.alert_whatsapp;
  doc["profile_synced_at"]  = td.profile_synced_at;
  String out;
  serializeJson(doc, out);
  return out;
}

// ─────────────────────────────────────────────
//  WEB SERVER ROUTES
// ─────────────────────────────────────────────
void setupWebServer() {
  // Serve dashboard HTML from LittleFS
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Live JSON status endpoint
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildStatusJson());
  });

  // Vaccine profile update endpoint (called by Phase 2 RPC or local form)
  server.on("/api/profile", HTTP_POST, [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
        td.vaccine_name     = doc["vaccine_name"]     | td.vaccine_name;
        td.vaccine_temp_min = doc["vaccine_temp_min"] | td.vaccine_temp_min;
        td.vaccine_temp_max = doc["vaccine_temp_max"] | td.vaccine_temp_max;
        td.vaccine_stock    = doc["vaccine_stock"]    | td.vaccine_stock;
        td.profile_synced_at = millis();
        saveVaccineProfile();
        req->send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        req->send(400, "application/json", "{\"status\":\"bad json\"}");
      }
    }
  );

  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("[Web] Server started");
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[CoolCycle] Booting v" + String(FW_VERSION));

  // Output pins
  pinMode(PIN_BUZZER,    OUTPUT);
  pinMode(PIN_LED_RED,   OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  digitalWrite(PIN_LED_RED,   LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_BUZZER,    LOW);

  // Input pins
  pinMode(PIN_REED, INPUT);   // External 10kΩ pull-up, no internal pull-up on GPIO34
  pinMode(PIN_CONFIG_BTN, INPUT_PULLUP);

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] Mount failed! Formatting...");
  } else {
    Serial.println("[LittleFS] Mounted OK");
    loadVaccineProfile();
  }
  
  loadLastGPS();

  prefs.begin("coolcycle", false);
  charge_cycle_count = prefs.getInt("cycles", 0);
  
  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);

  // I²C
  Wire.begin(21, 22);

  // DS18B20
  ds18b20.begin();
  Serial.printf("[DS18B20] Found %d sensor(s)\n", ds18b20.getDeviceCount());

  // DHT22
  dht.begin();

  // INA219 × 2
  if (!ina_battery.begin()) Serial.println("[INA219] Battery sensor not found!");
  else                       ina_battery.setCalibration_32V_2A();
  if (!ina_solar.begin())   Serial.println("[INA219] Solar sensor not found!");
  else                       ina_solar.setCalibration_32V_2A();

  // DS3231 RTC
  if (!rtc.begin()) Serial.println("[DS3231] RTC not found!");
  if (rtc.lostPower()) {
    Serial.println("[DS3231] Power lost — set time from compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // BH1750
  if (bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("[BH1750] Light sensor OK");
  }

  // GPS on UART1 (RX=18, TX=19)
  gpsSerial.begin(9600, SERIAL_8N1, 18, 19);
  Serial.println("[GPS] Neo-6M on UART1 (RX=18, TX=19)");

  // WiFi — Captive Portal via WiFiManager
  WiFiManager wm;
  wm.setConnectTimeout(20);
  Serial.println("[WiFi] Starting WiFiManager...");
  if (!wm.autoConnect(AP_SSID)) {
    Serial.println("[WiFi] Failed to connect, running in AP mode");
  } else {
    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
  }

  // Web server
  setupWebServer();

  // Boot LED flash
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED_GREEN, HIGH); delay(100);
    digitalWrite(PIN_LED_GREEN, LOW);  delay(100);
  }
  Serial.println("[CoolCycle] Boot complete.");
}

// ─────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────
void loop() {
  esp_task_wdt_reset();
  unsigned long now = millis();
  
  // ── 0. CONFIG BUTTON ──
  if (digitalRead(PIN_CONFIG_BTN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(PIN_CONFIG_BTN) == LOW) {
      unsigned long btn_start = millis();
      while(digitalRead(PIN_CONFIG_BTN) == LOW) { esp_task_wdt_reset(); delay(10); }
      unsigned long pressed_time = millis() - btn_start;
      
      if (pressed_time > 5000) {
        Serial.println("[Button] Long press -> Resetting WiFi and Rebooting");
        WiFiManager wm;
        wm.resetSettings();
        delay(1000);
        ESP.restart();
      } else {
        Serial.println("[Button] Short press -> Silencing alarms locally");
        digitalWrite(PIN_BUZZER, LOW);
      }
    }
  }

  // ── 1. POWER (every 1s) ──
  if (now - t_power >= INTERVAL_POWER) {
    t_power = now;

    td.battery_voltage = ina_battery.getBusVoltage_V();
    td.battery_current = ina_battery.getCurrent_mA() / 1000.0f; // convert to A
    updateBatterySOC();

    td.solar_voltage   = ina_solar.getBusVoltage_V();
    td.solar_current   = ina_solar.getCurrent_mA() / 1000.0f;
    td.solar_power     = td.solar_voltage * td.solar_current;

    td.compressor_on   = (fabs(td.battery_current) > COMPRESSOR_MIN_AMPS);
    td.remaining_cool_h = calcRemainingCoolHours();
    td.rssi            = WiFi.RSSI();
    td.esp32_temp_c    = (temprature_sens_read() - 32) / 1.8f;
  }

  // ── 2. SENSORS (every 5s) ──
  if (now - t_sensors >= INTERVAL_SENSORS) {
    t_sensors = now;

    ds18b20.requestTemperatures();
    float ds_temp = ds18b20.getTempCByIndex(0);
    if (ds_temp != DEVICE_DISCONNECTED_C) td.temp_internal = ds_temp;

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      td.humidity     = h;
      td.temp_ambient = t;
    }

    td.solar_lux = bh1750.readLightLevel();
  }
  
  // ── 2b. SOLAR ROLLING AVERAGE (every 60s) ──
  static unsigned long t_solar_hist = 0;
  if (now - t_solar_hist >= 60000) {
    t_solar_hist = now;
    solar_history[solar_history_idx] = td.solar_power;
    solar_history_idx++;
    if (solar_history_idx >= 15) {
      solar_history_idx = 0;
      solar_history_filled = true;
    }
  }

  // ── 3. DOOR (every 500ms) ──
  if (now - t_door >= INTERVAL_DOOR) {
    t_door = now;
    bool doorNow = (digitalRead(PIN_REED) == HIGH); // HIGH = magnet absent = open
    if (doorNow && !td.door_open) {
      door_opened_at = now;
    }
    td.door_open = doorNow;
    td.door_open_secs = doorNow ? (int)((now - door_opened_at) / 1000) : 0;
  }

  // ── 4. GPS (every 10s — reads NMEA continuously) ──
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
  if (now - t_gps >= INTERVAL_GPS) {
    t_gps = now;
    if (gps.location.isValid()) {
      td.latitude  = gps.location.lat();
      td.longitude = gps.location.lng();
      if (!td.gps_valid) {
        td.gps_valid = true;
        saveLastGPS();
      }
    }
  }

  // ── 5. LOCAL ALARMS (every 2s) ──
  if (now - t_alarms >= INTERVAL_ALARMS) {
    t_alarms = now;
    handleLocalAlarms();
  }

  // ── 6. OFFLINE LOG (every 60s if cloud offline) ──
  if (now - t_log >= INTERVAL_LOG) {
    t_log = now;
    if (!td.cloud_online) {
      appendOfflineLog();
    }
  }

  // ── Phase 2 STUB: MQTT publish (every 30s) ──
  // Uncomment and implement when SIM800L + PubSubClient added
  // if (now - t_mqtt >= INTERVAL_MQTT) {
  //   t_mqtt = now;
  //   publishToThingsBoard();
  // }
}
