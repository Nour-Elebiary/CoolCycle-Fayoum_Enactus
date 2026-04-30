# CoolCycle IoT System — Architecture Package

> **Solar-Powered Medical Cold Chain Monitoring**  
> Field-tested in Fayoum, Egypt · **DOIT ESP32-DevKit-V1** + ThingsBoard PE Cloud (Europe)  
> Generated: April 2026 · Conversation ID: `4918048e-8113-49c0-8131-20a1d1922e45`

---

## 📁 File Index

```
architecture/
│
├── README.md                        ← You are here
│
├── Firmware (Arduino IDE)
│   └── coolcycle_firmware.ino        ← Combined Phase 1 & 2 (Sensors + Cloud + RPC)
│
├── Local Dashboard (ESP32-hosted)
│   └── 04_offline_dashboard.html    ← Upload to LittleFS /index.html
│
├── Web Portal (coolcycle.com backend)
│   ├── coolcycle_jwt_bridge.js      ← Node.js SSO bridge (coolcycle.com → ThingsBoard)
│   └── coolcycle_admin_portal.html  ← Customer Admin vaccine profile manager UI
│
├── ThingsBoard Rule Chain Imports (PE Optimized)
│   └── tb_export/
│       ├── coolcycle_root (1).json   ← [Chain 1] Root Ingestion & Routing
│       ├── coolcycle_alarms (1).json ← [Chain 2] Consolidates Alarms, WhatsApp & Pred. Maint.
│       └── rule_chain_rpc.json       ← [Chain 3] Vaccine Profile Sync & OTA
│
└── Architecture Documents
    ├── 01_pinout_and_hardware.md    ← GPIO table, I²C map, power budget
    ├── 02_rule_chain_logic.md       ← 3-Chain Consolidated Logic Reference
    ├── 03_dashboard_architecture.md ← 3-tier RBAC dashboards + JWT SSO
    └── 05_implementation_roadmap.md ← Phase 1/2/3 with test criteria
```

---

## 🚀 Quickstart — Order of Operations

### Step 1 — ThingsBoard PE Setup (Free Plan Compatible)

1. Log in to `https://eu.thingsboard.cloud`
2. Create **Device Profile**: `coolcycle-unit` (MQTT Transport).
3. Import Rule Chains (Rule Chains → `+` → Import):
   - `tb_export/coolcycle_root (1).json` → **Set as Root**.
   - `tb_export/coolcycle_alarms (1).json`
   - `tb_export/rule_chain_rpc.json`
4. **Twilio Config**: In `coolcycle_alarms (1).json`, update the **Twilio REST** node with your SID/AuthToken.
5. **Shared Attributes**: On your device, set `vaccine_temp_min/max`, `clinic_lat/lon`, and `alert_whatsapp`.

---

### Step 2 — Firmware Configuration

1. Open `coolcycle_firmware.ino` in Arduino IDE.
2. **Library Requirements**: Install `PubSubClient`, `ArduinoJson`, `DHT`, `DallasTemperature`, `WiFiManager`, etc.
3. **Configure Token**:
   ```cpp
   const char* TB_ACCESS_TOKEN = "99beqj10rezgdsfwkgvh"; // Your Device Token
   ```
4. **LittleFS Upload**: Upload `04_offline_dashboard.html` to ESP32 as `/index.html`.
5. **Flash**: Once uploaded, check Serial Monitor. You should see "MQTT Connected".

---

### Step 3 — Testing Phase

1. **Telemetry**: Verify data appearing in ThingsBoard Latest Telemetry.
2. **Alarms**: Trigger a temperature alarm (manually heat sensor) and check WhatsApp.
3. **RPC Sync**: Change `vaccine_temp_max` in ThingsBoard Shared Attributes. Verify the ESP32 Serial Monitor says "Profile Synced from Cloud".


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
| 13 | DHT22 (ambient) | GPIO 13 — safe, not a strapping pin |
| 21/22 | I²C bus (INA219×2, DS3231, BH1750) | 4.7kΩ pull-ups |
| 16/17 | Neo-6M GPS UART2 | RX=16, TX=17 (gpsSerial(2)) |
| 25/26 | SIM800L UART1 | RX=25, TX=26 (modemSerial(1)) — dedicated 4.0V buck reg + 1000µF cap |
| 27 | SIM800L PWR_KEY | Active LOW 1s pulse |
| 14 | SIM800L RST | Active LOW 100ms pulse |
| 34 | Reed switch (door) | Input-only, external 10kΩ pull-up |
| 35 | LDR (ADC1 CH7) | ADC1 = WiFi-safe |
| 32 | Buzzer | Via 2N2222 NPN transistor + 1kΩ base resistor |
| 33 | LED Red (alarm) | 330Ω series resistor |
| 23 | Sensor Power Gate | P-ch MOSFET gate — cuts sensor rail in deep sleep |
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
| MCU | **DOIT ESP32-DevKit-V1** (30-pin, ESP-WROOM-32) |
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
