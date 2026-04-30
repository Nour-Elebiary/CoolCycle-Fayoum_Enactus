# CoolCycle — Deliverable 5: Implementation Roadmap

> **Three-Phase delivery. Each phase is independently deployable and testable.**

---

## Phase 1: Local Sensing & Offline Dashboard

**Duration**: 2–3 weeks  
**Goal**: A fully standalone unit that monitors, logs, alarms locally — no internet required.

### 1.1 Firmware Setup

**Libraries (Arduino Core):**
```
DallasTemperature + OneWire   → DS18B20
DHT sensor library (Adafruit) → DHT22
Adafruit INA219               → INA219 × 2
RTClib                        → DS3231
BH1750                        → BH1750 lux sensor
TinyGPSPlus                   → Neo-6M NMEA parsing
ESPAsyncWebServer             → Local HTTP server
AsyncTCP                      → Async dependency
ArduinoJson                   → JSON serialization
LittleFS                      → Filesystem for profiles + logs
```

**Firmware Architecture (non-blocking):**
```cpp
// Timer schedule (all millis()-based, no delay())
#define INTERVAL_SENSORS   5000   // DS18B20, DHT22, BH1750
#define INTERVAL_POWER     1000   // INA219 × 2
#define INTERVAL_GPS      10000   // Neo-6M NMEA
#define INTERVAL_RTC      60000   // DS3231 sync
#define INTERVAL_DOOR       500   // Reed switch poll
#define INTERVAL_MQTT     30000   // ThingsBoard publish (Phase 2)
#define INTERVAL_WEBUPD    5000   // Local JSON status refresh
```

**`/api/status` JSON Response (served to dashboard):**
```json
{
  "temp_internal": 4.2,
  "temp_ambient": 28.5,
  "humidity": 62.1,
  "battery_voltage": 13.1,
  "battery_current": -2.3,
  "battery_soc": 78,
  "solar_voltage": 17.4,
  "solar_current": 3.8,
  "solar_power": 65.9,
  "door_open": false,
  "door_open_secs": 0,
  "compressor_on": true,
  "remaining_cool_hours": 14.2,
  "latitude": 29.3084,
  "longitude": 30.8428,
  "vaccine_name": "OPV",
  "vaccine_temp_min": 2.0,
  "vaccine_temp_max": 8.0,
  "vaccine_stock": 120,
  "clinic_name": "El Nazla Clinic",
  "alert_whatsapp": "201001234567",
  "profile_synced_at": 1714000000000,
  "cloud_online": false
}
```

**Remaining Cool Time Algorithm:**
```cpp
// Battery capacity at current SOC (Ah)
float capacity_remaining_Ah = (battery_soc / 100.0) * BATTERY_CAPACITY_AH; // e.g. 50Ah
// Compressor draw from INA219 (W)
float compressor_draw_W = fabs(battery_current) * battery_voltage;
// Solar compensation
float net_draw_W = max(0.0f, compressor_draw_W - solar_power);
// Hours remaining
float hours = (net_draw_W > 0)
  ? (capacity_remaining_Ah * battery_voltage) / net_draw_W
  : 99.0;
remaining_cool_hours = min(hours, 99.0f);
```

### 1.2 Soft-AP + Local WiFi Config

```cpp
// Dual-mode: connect to clinic WiFi + broadcast own AP
WiFi.mode(WIFI_AP_STA);
WiFi.softAP("CoolCycle-CC0001", "coolcycle123");
WiFi.begin(CLINIC_SSID, CLINIC_PASS);
// Dashboard accessible at:
//   192.168.4.1        (via Soft-AP, no internet)
//   <dhcp-ip>          (via clinic WiFi)
```

### 1.3 LittleFS File Structure

```
/
├── index.html          ← The offline dashboard (04_offline_dashboard.html)
├── config/
│   └── vaccine.json    ← Vaccine profile (pulled from cloud shared attrs)
└── log/
    └── readings.json   ← Offline telemetry log (for backdate sync)
```

### 1.4 Local Alarm Triggers

| Condition | Action |
|-----------|--------|
| temp > vaccine_temp_max | Buzzer ON, Red LED ON |
| temp < vaccine_temp_min | Buzzer ON, Red LED ON |
| battery_soc < 15% | Buzzer 3× beep |
| door_open > 60s | Buzzer intermittent |
| All clear | Buzzer OFF, Red LED OFF |

### Phase 1 Test Criteria
- [ ] All sensors read valid values in Serial Monitor
- [ ] `/api/status` returns correct JSON at `192.168.4.1/api/status`
- [ ] Dashboard loads on mobile browser via Soft-AP
- [ ] Gauges update every 5 seconds without page refresh
- [ ] Buzzer triggers when temp probe placed in >8°C environment
- [ ] Offline log writes to LittleFS correctly

---

## Phase 2: Cloud Integration

**Duration**: 2–3 weeks  
**Goal**: Device registered on ThingsBoard PE, real-time MQTT telemetry, rule chains active.

### 2.1 ThingsBoard PE Setup (Europe Cloud)

**Step-by-step:**
```
1. Create Tenant: "CoolCycle"
2. Create Device Profile: "coolcycle-unit"
   - Transport: MQTT
   - Queue: HighPriority
   - Alarm Rules: (from 06_alarms_notifications_device_management.md patterns)
3. Create Device: "CC-0001"
   - Copy Access Token
4. Import Rule Chains (from 02_rule_chain_logic.md):
   - Root Ingestion
   - CoolCycle Alarms
   - Vaccine Profile Sync
   - WhatsApp/Twilio REST
   - Predictive Maintenance
   - OTA Trigger
5. Create Assets: Region → Clinic
6. Create Relations: Clinic → Contains → CC-0001
7. Create Customers: "El Nazla Health Unit"
8. Assign devices to customer
9. Create Dashboards (from 03_dashboard_architecture.md):
   - Developer Fleet Dashboard
   - Customer Admin Dashboard
   - Clinic Staff Dashboard
10. Configure RBAC (3 user groups, roles, permissions)
11. Configure White Labeling: CoolCycle branding
```

### 2.2 Cellular (SIM800L) Firmware Module

```cpp
// SIM800L initialization sequence (UART1: RX=25, TX=26)
void initCellular() {
  Serial1.begin(9600, SERIAL_8N1, 25, 26); // RX=25, TX=26
  sendAT("AT", 1000);           // Check alive
  sendAT("AT+CPIN?", 2000);     // Check SIM
  sendAT("AT+CREG?", 2000);     // Network registration
  sendAT("AT+CGATT=1", 5000);   // Attach GPRS
  sendAT("AT+CSTT=\"data.vodafone.net.eg\"", 2000); // APN (Vodafone Egypt)
  sendAT("AT+CIICR", 5000);     // Bring up wireless
  sendAT("AT+CIFSR", 2000);     // Get IP
}
// GPS is on UART2: Serial2.begin(9600, SERIAL_8N1, 16, 17) — handled by gpsSerial(2)
// Then use AT+CIPSTART to open TCP to thingsboard.cloud:1883
// and forward MQTT packets manually via AT+CIPSEND
```

### 2.3 Shared Attribute Subscription (Vaccine Profile Pull)

```cpp
// Subscribe on connect
client.subscribe("v1/devices/me/attributes/response/+");
client.subscribe("v1/devices/me/attributes");

// Request shared attributes on boot
String req = "{\"sharedKeys\":\"vaccine_name,vaccine_temp_min,"
             "vaccine_temp_max,vaccine_stock,clinic_lat,clinic_lon,"
             "geofence_radius_m,alert_whatsapp\"}";
client.publish("v1/devices/me/attributes/request/1", req.c_str());

// On receive: parse JSON → write to /config/vaccine.json on LittleFS
void onSharedAttrUpdate(JsonObject attrs) {
  File f = LittleFS.open("/config/vaccine.json", "w");
  serializeJson(attrs, f);
  f.close();
  profile_synced_at = rtc.now().unixtime() * 1000ULL;
}
```

### Phase 2 Test Criteria
- [ ] Device appears ACTIVE on ThingsBoard with all telemetry keys visible
- [ ] Alarm fires on ThingsBoard when probe is warmed above 8°C
- [ ] WhatsApp message received within 30s of alarm trigger
- [ ] Vaccine profile update from Admin Dashboard reflected in `/api/status` within 60s
- [ ] Geofence alert fires when GPS moved > radius
- [ ] OTA shared attributes pushed → device logs `fw_state: DOWNLOADING`

---

## Phase 3: Deployment & OTA

**Duration**: 2 weeks  
**Goal**: Field-ready units, remote update capability, clinic staff trained.

### 3.1 OTA Firmware Update Flow

```cpp
// Device checks for OTA on shared attribute update
void checkOTA(JsonObject attrs) {
  if (attrs.containsKey("fw_version")) {
    String newVer = attrs["fw_version"];
    if (newVer != CURRENT_FW_VERSION) {
      String url = attrs["fw_url"];
      publishAttr("fw_state", "DOWNLOADING");
      // Use HTTPClient to download binary
      // Write to OTA partition via Update.h
      if (Update.begin(contentLength)) {
        // stream download...
        if (Update.end()) {
          publishAttr("fw_state", "UPDATED");
          ESP.restart();
        }
      }
    }
  }
}
```

### 3.2 Deployment Checklist (Per Unit)

```
□ Flash latest firmware with device-specific ACCESS_TOKEN
□ Configure clinic WiFi credentials in firmware (or captive portal)
□ Set device server attributes: clinic_name, clinic_lat, clinic_lon
□ Assign to correct customer on ThingsBoard
□ Test cellular connectivity (check RSSI, APN)
□ Verify /api/status accessible at 192.168.4.1
□ Set WhatsApp alert number in shared attributes
□ Train clinic staff: show dashboard, explain icons
□ Train IT admin: vaccine profile update, alarm acknowledgment
□ Register technician WhatsApp for alert_whatsapp field
```

### 3.3 Technician OTA Trigger (Remote)

From Customer Admin Dashboard:
1. Navigate to device detail
2. Click **Trigger OTA Update**
3. Select firmware version from dropdown
4. ThingsBoard writes `fw_title`, `fw_version`, `fw_url`, `fw_checksum` as shared attrs
5. Device detects change → downloads → restarts → reports `fw_state: UPDATED`

### Phase 3 Test Criteria
- [ ] OTA update completes without physical access to device
- [ ] Device comes back online with new `fw_version` reported
- [ ] Clinic staff can view dashboard on their phone via Soft-AP without any setup
- [ ] All 3 dashboard tiers verified with real login credentials
- [ ] WhatsApp alert received by clinic contact within 30s of critical alarm

---

## Technology Stack Summary

| Layer | Technology | Notes |
|-------|-----------|-------|
| MCU | **DOIT ESP32-DevKit-V1** (30-pin, ESP-WROOM-32) | Dual-core, 4MB flash |
| Firmware | Arduino Core for ESP32 | AsyncWebServer, PubSubClient, ArduinoJson |
| Storage | LittleFS | Vaccine profiles, offline logs |
| Cellular | SIM800L or SIM7600 | GPRS/4G, APN: Vodafone Egypt |
| Cloud | ThingsBoard PE — Europe | thingsboard.cloud/eu |
| Protocol | MQTT 3.1.1 (port 1883 / 8883 TLS) | Access Token auth |
| Scripting | TBEL (ThingsBoard Expression Language) | Rule Engine scripts |
| WhatsApp | Twilio WhatsApp Business API | REST API Call node |
| GPS | Neo-6M + TinyGPSPlus | UART2 (RX=16, TX=17), NMEA sentences |
| OTA | ThingsBoard OTA + ESP Arduino Update.h | fw_* shared attributes |
| Local UI | HTML/CSS/JS (04_offline_dashboard.html) | Served by AsyncWebServer |

