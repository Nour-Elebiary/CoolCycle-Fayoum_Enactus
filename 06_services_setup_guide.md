# CoolCycle — Third-Party Services Setup Guide

> Configure Twilio WhatsApp, LittleFS upload, and ThingsBoard cloud in the correct order.

---

## 1. Twilio WhatsApp Business API Setup

### 1.1 Create Twilio Account
1. Go to [twilio.com](https://www.twilio.com) → Sign up (free trial works for testing)
2. From Console Dashboard, note your:
   - **Account SID**: `ACxxxxxxxxxxxxxxxxxxxxxxxxxxxx`
   - **Auth Token**: `xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`

### 1.2 Enable WhatsApp Sandbox (Testing)
1. Twilio Console → Messaging → Try it out → Send a WhatsApp message
2. Your Sandbox number: `+1 415 523 8886`
3. Clinic tech sends `join <sandbox-keyword>` to that number to opt in
4. Test alert will come from that sandbox number

### 1.3 Production WhatsApp Business (Deployment)
1. Twilio Console → Messaging → Senders → WhatsApp Senders → Register
2. Submit business verification (takes 1–3 business days)
3. Once approved, your number replaces sandbox number in rule chain

### 1.4 Update Rule Chain with Twilio Credentials
In `tb_export/rule_chain_whatsapp.json`, replace:

| Placeholder | Replace With |
|-------------|-------------|
| `REPLACE_TWILIO_SID` | Your Account SID (e.g. `AC1234...`) |
| `REPLACE_WITH_BASE64_SID_AUTHTOKEN` | `base64("ACxxxx:authtoken")` |

**Generate the Base64 value:**
```bash
# PowerShell
[Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("ACxxxxx:your_auth_token"))

# Linux/macOS
echo -n "ACxxxxx:your_auth_token" | base64
```

**Alert recipient number** — set as shared attribute `alert_whatsapp` on each device:
```
Value: "201001234567"    ← no + sign, no spaces
```

---

## 2. LittleFS — Upload Offline Dashboard to ESP32

The offline dashboard HTML must be uploaded to ESP32 flash storage so `AsyncWebServer` can serve it.

### 2.1 Install the LittleFS Upload Plugin

**Arduino IDE 1.x:**
1. Download [arduino-esp32fs-plugin](https://github.com/lorol/arduino-esp32fs-plugin/releases)
2. Extract to `~/Arduino/tools/ESP32FS/tool/esp32fs.jar`
3. Restart Arduino IDE
4. Menu: **Tools → ESP32 LittleFS Data Upload**

**Arduino IDE 2.x:**
1. Download [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload/releases) `.vsix`
2. In IDE 2: `Ctrl+Shift+P` → `Install from VSIX`
3. Use `Ctrl+Shift+P` → `Upload LittleFS to Pico/ESP8266/ESP32`

### 2.2 Prepare the `data/` Folder

```
your_sketch_folder/
├── coolcycle_firmware.ino
├── coolcycle_phase2_mqtt.ino
└── data/
    ├── index.html              ← copy of 04_offline_dashboard.html
    └── config/
        └── vaccine.json        ← optional pre-seeded profile
```

**vaccine.json pre-seed (optional):**
```json
{
  "vaccine_name": "OPV",
  "vaccine_temp_min": 2.0,
  "vaccine_temp_max": 8.0,
  "vaccine_stock": 0,
  "clinic_name": "El Nazla Clinic",
  "alert_whatsapp": "201001234567",
  "profile_synced_at": 0
}
```

### 2.3 Upload Steps
1. Open sketch in Arduino IDE
2. **Do NOT have Serial Monitor open**
3. Select correct Board: `ESP32 Dev Module` and Port
4. Tools → **ESP32 LittleFS Data Upload**
5. Wait for `[LittleFS] upload complete`
6. Then flash the firmware normally (Upload button)

### 2.4 Verify Upload
Open Serial Monitor after boot:
```
[LittleFS] Mounted OK
[LittleFS] Vaccine profile loaded: OPV (2.0-8.0°C)
[Web] Server started
```
Browse to `http://192.168.4.1` — dashboard should load.

---

## 3. ThingsBoard PE — Cloud Setup Checklist

Follow this exact order to avoid dependency issues:

```
□ 1. Create Device Profile: "coolcycle-unit"
      → Import: tb_export/device_profile_coolcycle_unit.json
      → Note Provision Key + Secret for provisioning sketch

□ 2. Import Rule Chains (Rule Chains → + → Import):
      a. tb_export/rule_chain_root.json        ← set as ROOT after import
      b. tb_export/rule_chain_alarms.json
      c. tb_export/rule_chain_whatsapp.json    ← add Twilio creds first

□ 3. Link rule chains:
      → Open root chain → find "CoolCycle Alarms" node
      → Update ruleChainId to alarm chain's actual UUID
      → Open alarm chain → find "WhatsApp Alert Chain" node
      → Update ruleChainId to whatsapp chain's UUID

□ 4. Create Asset hierarchy:
      Asset Profile: "region" → Asset: "Fayoum Region"
      Asset Profile: "clinic" → Asset: "El Nazla Clinic"
      Relation: El Nazla Clinic --[Contains]--> (devices added later)

□ 5. Provision first device:
      → Flash coolcycle_provisioning.ino with PROVISION_KEY + DEVICE_NAME
      → Buzzer beeps 6 times = success → device appears in TB Devices list

□ 6. Set device shared attributes (Devices → CC-0001 → Attributes → Shared):
      vaccine_name:       OPV
      vaccine_temp_min:   2
      vaccine_temp_max:   8
      vaccine_stock:      120
      clinic_name:        El Nazla Clinic
      clinic_lat:         29.3084
      clinic_lon:         30.8428
      geofence_radius_m:  300
      alert_whatsapp:     201001234567

□ 7. Create relation: El Nazla Clinic --[Contains]--> CC-0001

□ 8. Create Customer: "El Nazla Health Unit"
      → Assign CC-0001 to this customer

□ 9. Create Users:
      IT Admin (Customer Admin role) → assign Admin Dashboard
      Clinic Staff (Customer User role) → assign Clinic Dashboard

□ 10. Create Dashboards (use 03_dashboard_architecture.md as spec):
       Tier 1 Developer Dashboard
       Tier 2 Customer Admin Dashboard
       Tier 3 Clinic Staff Dashboard
       → Assign Tier 2 & 3 to "El Nazla Health Unit" customer

□ 11. Configure White Labeling (PE):
       Settings → White Labeling → Upload CoolCycle logo
       Set primary color: #00d4aa (teal)

□ 12. Flash main firmware (Phase 2):
       Fill TB_ACCESS_TOKEN from /config/device.json on LittleFS
       Uncomment setupPhase2() and loopPhase2() in firmware
       Flash → device connects and pushes telemetry

□ 13. Enable Iframe Embedding for CoolCycle Website:
       Go to System Settings → Security Settings
       Add `https://coolcycle.com` to allowed X-Frame-Options
       Add `frame-ancestors 'self' https://coolcycle.com;` to CSP

□ 14. Setup Public Dashboard Link (For User Portal Integration):
       Go to Dashboards → Select the Tier 3 Clinic Staff Dashboard
       Click "Make Public" (icon with the globe)
       Copy the Public Link generated
       Go to Customers → "El Nazla Health Unit" → Attributes → Server attributes
       Add new String attribute: key=`public_dashboard_link`, value=`<Pasted Public Link>`
```

---

## 4. SIM Card Setup (Vodafone Egypt)

| Setting | Value |
|---------|-------|
| APN | `internet.vodafone.net` |
| Username | *(leave empty)* |
| Password | *(leave empty)* |
| SIM type | Data SIM (no voice required) |
| Data plan | Minimum 500MB/month per unit (telemetry is ~30KB/day) |

**Verify cellular in Serial Monitor:**
```
[SIM800L] Modem OK
[SIM800L] Signal quality: 18     ← > 10 is acceptable, > 20 is good
[SIM800L] IP: 41.42.xxx.xxx
[MQTT] Connected to ThingsBoard
[MQTT] Telemetry published
```

---

## 5. Geofencing — Set Clinic Coordinates

For each new clinic deployment:

1. Find clinic GPS coordinates (use Google Maps → right-click → copy coordinates)
2. Set shared attributes on the device:
   ```
   clinic_lat: 29.3084
   clinic_lon: 30.8428
   geofence_radius_m: 300
   ```
3. Alarm fires automatically if unit moves > 300m from these coordinates
4. WhatsApp message includes Google Maps link of actual position

---

## 6. OTA Firmware Update Workflow

When a new firmware version is ready:

```
1. Compile firmware in Arduino IDE → Sketch → Export Compiled Binary
   → Creates: coolcycle_firmware.ino.bin

2. Upload to ThingsBoard:
   OTA Updates → + → Upload Package
     Title: "CoolCycle Firmware"
     Version: "1.1.0"
     Device Profile: "coolcycle-unit"
     Type: FIRMWARE
     File: coolcycle_firmware.ino.bin

3. Assign to Device Profile (updates all units):
   Device Profiles → coolcycle-unit → Firmware → Select version

4. OR assign to single device only:
   Devices → CC-0001 → OTA Updates → Assign firmware

5. Monitor update progress:
   Devices → CC-0001 → Attributes → Client
   Watch fw_state: DOWNLOADING → DOWNLOADED → VERIFIED → UPDATED
```

---

## 7. Estimated Monthly Data Usage Per Unit

| Source | Frequency | Size | Monthly |
|--------|-----------|------|---------|
| MQTT telemetry | Every 30s | ~0.8 KB | ~68 MB |
| Attribute sync | On change | ~0.2 KB | ~1 MB |
| Alarm messages | Occasional | ~0.3 KB | ~0.5 MB |
| OTA update | Monthly | ~1 MB | ~1 MB |
| **Total** | | | **~71 MB** |

> Vodafone Egypt 100MB data SIM (~15 EGP/month) is sufficient per unit.
