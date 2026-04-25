# CoolCycle IoT System — Architecture Package

> **Solar-Powered Medical Cold Chain Monitoring**  
> Field-tested in Fayoum, Egypt · ESP32 + ThingsBoard PE Cloud (Europe)  
> Generated: April 2026 · Conversation ID: `4918048e-8113-49c0-8131-20a1d1922e45`

---

## 📁 File Index

```
architecture/
│
├── README.md                        ← You are here
│
├── Firmware (Arduino IDE)
│   ├── coolcycle_firmware.ino        ← Phase 1: All sensors + offline dashboard
│   ├── coolcycle_phase2_mqtt.ino     ← Phase 2: SIM800L + MQTT + OTA (add to same folder)
│   └── coolcycle_provisioning.ino    ← Run ONCE on new unit to register on ThingsBoard
│
├── Local Dashboard (ESP32-hosted)
│   └── 04_offline_dashboard.html    ← Upload to LittleFS /index.html
│
├── Web Portal (coolcycle.com backend)
│   ├── coolcycle_jwt_bridge.js      ← Node.js SSO bridge (coolcycle.com → ThingsBoard)
│   └── coolcycle_admin_portal.html  ← Customer Admin vaccine profile manager UI
│
├── ThingsBoard Rule Chain Imports
│   └── tb_export/
│       ├── rule_chain_root.json     ← Import first (root chain)
│       ├── rule_chain_alarms.json   ← Import second (alarm logic)
│       └── rule_chain_whatsapp.json ← Import third (Twilio alerts)
│
└── Architecture Documents
    ├── 01_pinout_and_hardware.md    ← GPIO table, I²C map, power budget
    ├── 02_rule_chain_logic.md       ← All 6 rule chains + TBEL scripts
    ├── 03_dashboard_architecture.md ← 3-tier RBAC dashboards + JWT SSO
    └── 05_implementation_roadmap.md ← Phase 1/2/3 with test criteria
```

---

## 🚀 Quickstart — Order of Operations

### Step 1 — ThingsBoard PE Setup (Europe Cloud)

1. Log in to `https://eu.thingsboard.cloud`
2. Create **Device Profile**: `coolcycle-unit`
   - Transport: MQTT
   - Queue: `HighPriority`
   - Provisioning: `Allow to create new devices`
   - Copy the **Provision Key** and **Provision Secret**
3. Import Rule Chains (Rule Chains → `+` → Import):
   - `tb_export/rule_chain_root.json` → set as **Root**
   - `tb_export/rule_chain_alarms.json`
   - `tb_export/rule_chain_whatsapp.json`
4. In `rule_chain_root.json`, update `REPLACE_WITH_ALARM_CHAIN_ID` to the alarm chain's UUID
5. In `rule_chain_whatsapp.json`, replace `REPLACE_TWILIO_SID` and `REPLACE_WITH_BASE64_SID_AUTHTOKEN`
6. Create **Assets**: Region → Clinic, link with `Contains` relation
7. Create **Customers** (one per clinic/NGO), assign devices
8. Create **Dashboards** (3 tiers per `03_dashboard_architecture.md`), assign to customers
9. Configure **RBAC** (3 user groups per `03_dashboard_architecture.md`)

---

### Step 2 — Provision Each ESP32 Unit

1. Open `coolcycle_provisioning.ino` in Arduino IDE
2. Fill in:
   ```cpp
   const char* PROVISION_KEY    = "from TB Device Profile";
   const char* PROVISION_SECRET = "from TB Device Profile";
   const char* DEVICE_NAME      = "CC-0001";   // unique per unit
   const char* PROV_WIFI_SSID   = "setup wifi";
   ```
3. Flash to ESP32 → Green LED steady = provisioned, Red blinking = failed
4. Token is saved to LittleFS `/config/device.json` automatically

---

### Step 3 — Flash Main Firmware (Phase 1 — Offline)

1. Copy both into same Arduino sketch folder:
   - `coolcycle_firmware.ino`
2. Fill in WiFi credentials:
   ```cpp
   const char* CLINIC_WIFI_SSID = "clinic-wifi-name";
   const char* CLINIC_WIFI_PASS = "password";
   ```
3. Upload `04_offline_dashboard.html` to ESP32 LittleFS as `/index.html`  
   (Use Arduino IDE LittleFS upload plugin or `esptool`)
4. Flash firmware
5. Open browser → `http://192.168.4.1` → dashboard loads ✅

---

### Step 4 — Add Phase 2 (Cloud + Cellular)

1. Add `coolcycle_phase2_mqtt.ino` to the same sketch folder
2. Fill in:
   ```cpp
   const char* APN             = "internet.vodafone.net";
   const char* TB_HOST         = "eu.thingsboard.cloud";
   const char* TB_ACCESS_TOKEN = "from /config/device.json on LittleFS";
   ```
3. In `coolcycle_firmware.ino` `setup()`, uncomment:
   ```cpp
   setupPhase2();
   ```
4. In `coolcycle_firmware.ino` `loop()`, uncomment:
   ```cpp
   loopPhase2();
   ```
5. Flash — device now syncs telemetry to ThingsBoard every 30s via SIM800L

---

### Step 5 — Deploy Admin Portal & JWT Bridge

```bash
# Install dependencies
npm install express node-fetch cors dotenv

# Create .env (see .env.example below)
cp .env.example .env

# Run bridge (port 3001)
node coolcycle_jwt_bridge.js
```

Open `coolcycle_admin_portal.html` in browser → login → manage vaccine profiles.

---

## ⚙️ Key Configuration Reference

### ThingsBoard Shared Attributes (pushed to each device)

| Attribute | Type | Example | Purpose |
|-----------|------|---------|---------|
| `vaccine_name` | string | `"OPV"` | Vaccine label on dashboard |
| `vaccine_temp_min` | float | `2.0` | Lower alarm threshold (°C) |
| `vaccine_temp_max` | float | `8.0` | Upper alarm threshold (°C) |
| `vaccine_stock` | int | `120` | Vial count (informational) |
| `clinic_name` | string | `"El Nazla Clinic"` | Shown on local dashboard |
| `clinic_lat` | float | `29.3084` | Geofencing home lat |
| `clinic_lon` | float | `30.8428` | Geofencing home lon |
| `geofence_radius_m` | int | `300` | Alert if unit moves > N metres |
| `alert_whatsapp` | string | `"201001234567"` | WhatsApp alert recipient |

### MQTT Telemetry Keys (published by firmware)

`temp_internal` · `temp_ambient` · `humidity` · `battery_voltage` · `battery_current` · `battery_soc` · `solar_voltage` · `solar_current` · `solar_power` · `solar_lux` · `door_open` · `door_open_secs` · `compressor_on` · `remaining_cool_hours` · `latitude` · `longitude` · `gps_valid` · `rssi` · `fw_version`

### GPIO Quick Reference

| GPIO | Sensor | Notes |
|------|--------|-------|
| 4 | DS18B20 (internal temp) | 4.7kΩ pull-up, waterproof, center-mounted |
| 5 | DHT22 (ambient) | Strapping pin, safe after boot |
| 21/22 | I²C bus (INA219×2, DS3231, BH1750) | 4.7kΩ pull-ups |
| 16/17 | SIM800L UART2 | RX/TX (Requires dedicated 4.0V buck reg + 1000µF cap) |
| 18/19 | Neo-6M GPS UART1 | RX/TX |
| 34 | Reed switch (door) | Input-only, external 10kΩ pull-up + 10nF debounce cap |
| 35 | LDR (ADC1 CH7) | ADC1 = WiFi-safe |
| 25 | Buzzer | Via transistor |
| 26/27 | LEDs red/green | 330Ω resistor |
| 23 | SIM800L PWR_KEY | Active LOW 1s pulse |
| 0 | Config Button | Momentary to GND for silencing alarms/reset |

---

## 🔑 Alarm Reference

| Alarm | Severity | Auto-Clear | WhatsApp |
|-------|----------|-----------|----------|
| Temp > vaccine_temp_max | CRITICAL | ✅ | ✅ |
| Temp < vaccine_temp_min | MAJOR | ✅ | ✅ |
| Battery < 15% | CRITICAL | ✅ | ✅ |
| Battery < 30% | MAJOR | ✅ | ❌ |
| Door open > 60s | WARNING | ✅ | ❌ |
| Device offline | MAJOR | ✅ | ✅ |
| Geofence exit | CRITICAL | Manual | ✅ |

---

## 📐 Dashboard Tier Summary & UI Mockups

### 1. Local Offline Dashboard (`04_offline_dashboard.html`)
This HTML file is served directly from the ESP32's internal web server.
**Key Features:**
- **Status Banner**: Displays `● Local` or dynamically turns red and warns `⚠️ Cloud Offline — [X] readings buffered locally` when cellular drops.
- **Dynamic Sparkline**: Renders a live graph of the last 30 minutes of internal temperatures, auto-resizing the green "Safe Zone" based on the exact vaccine profile requirements.
- **Visuals**: Modern dark mode UI with interactive gauges and battery progress bars.

### 2. Customer Admin Portal (`coolcycle_admin_portal.html`)
A Node.js/ThingsBoard JWT-backed dashboard for managing vaccine profiles.
**Key Features:**
- **Dynamic Provisioning**: Push exact Min/Max temperature bounds and stock counts over MQTT directly to the physical ESP32.
- **Actionable Status**: Live indicator showing exactly when a device was last active.

| Tier | User | Access | Default Dashboard |
|------|------|--------|------------------|
| 1 | CoolCycle Developer | Full TB access | Fleet Command Center |
| 2 | IT Admin (Customer) | Own devices only | Customer Admin Dashboard |
| 3 | Clinic Staff | Read-only | Clinic Staff Monitor |

---

## 🧩 Technology Stack

| Layer | Technology |
|-------|-----------|
| MCU | ESP32-DevKit-V4 (WROOM-32D) |
| Firmware IDE | Arduino Core for ESP32 |
| Local storage | LittleFS |
| Local server | ESPAsyncWebServer |
| Cellular | SIM800L (TinyGSM) |
| Cloud platform | ThingsBoard PE — Europe |
| Cloud protocol | MQTT 3.1.1 (port 1883 / 8883 TLS) |
| Rule scripting | TBEL (ThingsBoard Expression Language) |
| WhatsApp | Twilio WhatsApp Business API |
| GPS | Neo-6M + TinyGPSPlus |
| Web portal backend | Node.js + Express |
| OTA | ThingsBoard OTA + ESP `Update.h` |

---

## 🔒 Security Measures & Risks

**Pre-Testing Preparation & Data Protection:**
- **Encrypted Placeholders:** All sensitive data in this repository (e.g., `TB_ACCESS_TOKEN`, `TWILIO_ACCOUNT_SID`, `PROVISION_KEY`) have been replaced with `<ENCRYPTED_...>` placeholders. Never commit actual API keys or credentials to version control. Use `.env` files for Node.js and external configuration files for Arduino.
- **Hardware Security:** The ESP32 firmware is susceptible to physical flash extraction. For production deployments (>100 units), it is highly recommended to enable **ESP32 Flash Encryption and Secure Boot V2 (eFuse)** to prevent malicious actors from extracting the ThingsBoard MQTT credentials from the hardware.
- **Communication Security:** The firmware enforces TLS over MQTT (Port 8883) using GlobalSign Root Certificates to ensure all telemetry and RPC commands are encrypted in transit over the cellular network.

---

## 🗒️ Resumability

This package was generated in conversation `4918048e-8113-49c0-8131-20a1d1922e45`.  
Brain artifacts: `C:\Users\Asus\.gemini\antigravity\brain\4918048e-...\`  
KI used: `thingsboard_core_skill` (14 artifacts)

To continue in a new session, read:
1. This `README.md`
2. `brain/.../implementation_plan.md`
3. `brain/.../task.md` (all tasks complete ✅)
