/*
 * CoolCycle — Device Auto-Provisioning Sketch
 * 
 * Purpose: Run ONCE on a new hardware unit in the laboratory.
 * This registers the device on ThingsBoard PE, receives a unique access token,
 * and securely saves it to LittleFS (/config/device.json).
 * 
 * Get PROVISION_KEY/SECRET from:
 *   ThingsBoard → Device Profiles → coolcycle-unit → Provisioning tab
 */
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

const char* PROVISION_KEY    = "<ENCRYPTED_PROVISION_KEY>";
const char* PROVISION_SECRET = "<ENCRYPTED_PROVISION_SECRET>";
const char* DEVICE_NAME      = "CC-0001";
const char* TB_HOST          = "https://eu.thingsboard.cloud";
const char* PROV_WIFI_SSID   = "<ENCRYPTED_WIFI_SSID>";
const char* PROV_WIFI_PASS   = "<ENCRYPTED_WIFI_PASS>";

#define PIN_BUZZER 32
#define PIN_LED_RED   33

void blinkSuccess() {
  for (int i=0;i<6;i++){digitalWrite(PIN_BUZZER,HIGH);delay(200);digitalWrite(PIN_BUZZER,LOW);delay(200);}
  digitalWrite(PIN_BUZZER,HIGH);
}
void blinkError() {
  for (int i=0;i<10;i++){digitalWrite(PIN_LED_RED,HIGH);delay(150);digitalWrite(PIN_LED_RED,LOW);delay(150);}
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUZZER,OUTPUT); pinMode(PIN_LED_RED,OUTPUT);

  if (!LittleFS.begin(true)) { Serial.println("LittleFS failed"); blinkError(); return; }

  // Check if already provisioned
  if (LittleFS.exists("/config/device.json")) {
    File f = LittleFS.open("/config/device.json","r");
    StaticJsonDocument<256> d; deserializeJson(d,f); f.close();
    if (String(d["access_token"]|"").length()>0) {
      Serial.println("Already provisioned: " + String(d["access_token"]|""));
      blinkSuccess(); return;
    }
  }

  // Connect WiFi
  WiFi.begin(PROV_WIFI_SSID, PROV_WIFI_PASS);
  Serial.print("Connecting WiFi");
  for (int i=0;i<30&&WiFi.status()!=WL_CONNECTED;i++){delay(500);Serial.print(".");}
  if (WiFi.status()!=WL_CONNECTED){Serial.println("\nWiFi failed");blinkError();return;}
  Serial.println("\nWiFi OK: "+WiFi.localIP().toString());

  // Provision request
  HTTPClient http;
  http.begin(String(TB_HOST)+"/api/v1/provision");
  http.addHeader("Content-Type","application/json");

  StaticJsonDocument<256> req;
  req["deviceName"]=DEVICE_NAME;
  req["provisionDeviceKey"]=PROVISION_KEY;
  req["provisionDeviceSecret"]=PROVISION_SECRET;
  char buf[256]; serializeJson(req,buf);

  int code=http.POST(buf);
  String body=http.getString(); http.end();
  Serial.printf("HTTP %d: %s\n",code,body.c_str());

  if (code!=200) {
    Serial.println("\n[FAIL] Provisioning Request Rejected by ThingsBoard.");
    Serial.println("Criteria: HTTP Code must be 200.");
    blinkError();
    return;
  }

  StaticJsonDocument<512> resp; deserializeJson(resp,body);
  if (String(resp["status"]|"FAILURE")!="SUCCESS") {
    Serial.println("\n[FAIL] ThingsBoard returned FAILURE status.");
    Serial.println("Criteria: Check if DEVICE_NAME already exists or limits reached.");
    blinkError();
    return;
  }

  String token=resp["credentialsValue"]|"";
  if (token.length()==0) {
    Serial.println("\n[FAIL] No credentialsValue (Token) returned.");
    blinkError();
    return;
  }

  // Save to LittleFS
  LittleFS.mkdir("/config");
  File f=LittleFS.open("/config/device.json","w");
  StaticJsonDocument<256> save;
  save["device_name"]=DEVICE_NAME;
  save["access_token"]=token;
  serializeJson(save,f); f.close();

  Serial.println("\n=========================================");
  Serial.println("[SUCCESS] DEVICE PROVISIONED SUCCESSFULLY");
  Serial.println("=========================================");
  Serial.println("Device Name: " + String(DEVICE_NAME));
  Serial.println("Access Token: " + token);
  Serial.println("Criteria: Token securely written to /config/device.json.");
  Serial.println("Next Step: You may now flash 'coolcycle_firmware.ino'");
  blinkSuccess();
}

void loop() {}
