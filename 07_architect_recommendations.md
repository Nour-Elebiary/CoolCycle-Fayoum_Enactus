# CoolCycle — Architect's Recommendations & Suggestions

> **Author**: Senior IoT & Systems Architect (Antigravity AI)  
> **Based on**: Full project review — hardware specs, field context (Fayoum, Egypt),  
> business model, team task doc, ThingsBoard PE architecture, and Phase 2 submission.  
> **Date**: April 2026

---

## 🔴 Critical — Fix Before Deployment

### 1. SIM800L Power Rail Is a Show-Stopper If Done Wrong

The SIM800L draws **up to 2A in bursts** during GPRS transmission. If powered through the ESP32 DevKit's onboard LDO (max ~500mA), the unit will brownout, reset, or corrupt LittleFS mid-write.

**Recommendation:**
- Power SIM800L from a dedicated **4.0V buck-boost regulator** (e.g., MP1584, LM2596) fed directly from the battery.
- Add a **1000µF/10V electrolytic capacitor** directly on SIM800L VCC pin.
- Add a **100nF ceramic capacitor** in parallel for HF noise.
- Never share the SIM800L rail with any sensor or the ESP32.

---

### 2. Reed Switch on GPIO 34 Has No Internal Pull-Up — You Will Get Ghost Alarms

GPIO 34 is input-only with **no internal pull-up/down resistor**. A floating pin in a clinic environment (fluorescent lighting, inverters) will generate spurious door-open interrupts, causing false alarms and unnecessary buzzer triggers.

**Recommendation:**
- Add a **10kΩ resistor from GPIO34 to 3.3V** on the PCB/breadboard (do not rely on software pull-up — it doesn't exist on this pin).
- Add **10nF debounce capacitor** from GPIO34 to GND to filter contact bounce.
- In firmware, add a **minimum 50ms debounce** before registering a state change:
```cpp
// Replace the simple digitalRead with debounced read
bool readDoorDebounced() {
  static bool lastState = false;
  static unsigned long lastChange = 0;
  bool current = (digitalRead(PIN_REED) == HIGH);
  if (current != lastState && millis() - lastChange > 50) {
    lastState = current;
    lastChange = millis();
  }
  return lastState;
}
```

---

### 3. DS18B20 Placement — The Most Physically Critical Decision

The DS18B20 measures **internal vaccine compartment temperature** — this single reading determines whether an alarm fires or is cleared. If placed wrong, it measures air near the door seal, not the vaccine temperature.

**Recommendation:**
- Mount the DS18B20 probe in the **geometric center** of the storage compartment, not near walls or the cooling element.
- Use a **stainless steel waterproof probe** variant (DS18B20 in TO-92 will corrode in condensation).
- Consider using **two DS18B20 sensors** on the same 1-Wire bus (they support multiple devices): one center, one near door. Report both, alarm on either. Firmware already supports `getDeviceCount()`.

---

### 4. LiFePO4 SOC Estimation Is Dangerously Inaccurate Without Calibration

The current firmware uses a **linear voltage → SOC curve** on LiFePO4. LiFePO4 has an extremely flat discharge curve between 20%–90% SOC (voltage barely changes). This means the "remaining cool time" calculation and low battery alarm will both fire **too late or too early**.

**Recommendation:**
- Implement a **Coulomb Counting** method using the INA219 current readings:
```cpp
// In the 1-second INA219 loop:
float current_A = ina_battery.getCurrent_mA() / 1000.0f;
float delta_Ah = current_A * (INTERVAL_POWER / 1000.0f) / 3600.0f;
battery_charge_Ah -= delta_Ah;  // negative = discharging
battery_soc = constrain((battery_charge_Ah / BATTERY_CAPACITY_AH) * 100, 0, 100);
```
- Recalibrate `battery_charge_Ah` to `BATTERY_CAPACITY_AH` when solar current shows full charge (voltage > 14.4V for 30+ minutes).
- This gives **±2% SOC accuracy** vs ±20% for OCV method.

---

## 🟡 High Priority — Strongly Recommended

### 5. Add TLS (Port 8883) for MQTT in Production

The current firmware uses **port 1883 (unencrypted)**. In a medical device context transmitting patient-adjacent data, plain MQTT over GPRS is a compliance risk. Clinic IT staff or NGO auditors may flag this.

**Recommendation:**
- Use ThingsBoard's MQTT over TLS (port 8883).
- Load TB's CA certificate into firmware:
```cpp
#include <WiFiClientSecure.h>
WiFiClientSecure secureClient;
secureClient.setCACert(TB_CA_CERT);  // ThingsBoard root CA
PubSubClient mqttClient(secureClient);
// Then connect on port 8883
```
- For SIM800L, use AT+SSLOPT and AT+CIPSSL=1 commands before AT+CIPSTART.

---

### 6. The Remaining Cool Time Algorithm Needs a Minimum Load Floor

If `solar_power >= compressor_draw`, `net_draw_W` becomes 0 or negative, returning 99 hours. This is displayed to clinic staff. **Staff should not act on a 99h reading as if it means perpetual safety** — the solar panel could be shaded in minutes.

**Recommendation:**
- Cap the displayed value at a realistic maximum and add a clear label:
```cpp
// Use minimum assumed load floor of 5W even when solar covers all
float net_draw_w = max(5.0f, compressor_draw_w - solar_power);
remaining_cool_h = min((capacity_wh / net_draw_w), 72.0f);  // cap at 72h
```
- On the dashboard, show `"> 72 hrs"` rather than `"99 hrs"` to prevent over-confidence.
- Add a tooltip: *"Estimate assumes current solar conditions. Will update if solar changes."*

---

### 7. Implement a Watchdog Timer in Firmware

In rural deployments with power fluctuations, the ESP32 can lock up in a GPRS reconnect loop, a stuck I²C read, or a GPS NMEA parse. Without a watchdog, the unit freezes indefinitely — silent failure with no telemetry.

**Recommendation:**
- Enable the hardware watchdog timer:
```cpp
#include <esp_task_wdt.h>
// In setup():
esp_task_wdt_init(60, true);   // 60-second timeout, panic on timeout
esp_task_wdt_add(NULL);        // Add current task

// In loop() — call regularly:
esp_task_wdt_reset();
```
- The 60-second window covers worst-case GPRS reconnect (~30s) with margin.

---

### 8. GPS Lock Failure Handling — Units Will Often Boot Without Lock

The Neo-6M takes **30–120 seconds for cold start** and may never get a lock indoors. If `gps_valid = false`, the geofencing rule chain will always fail silently, and the WhatsApp support button GPS link will be `0,0`.

**Recommendation:**
- Store **last known good GPS** in LittleFS:
```cpp
// When GPS valid, persist to /config/lastgps.json
// On boot, load last known GPS as fallback
// In /api/status JSON, add "gps_source": "live" or "cached"
```
- In the geofencing TBEL script, check `msg.gps_valid == true` before computing distance — already implemented, but also add a `GPS Lost` alarm if `gps_valid` stays `false` for > 1 hour on a deployed unit.
- For clinic deployments where the unit never moves: set `geofence_radius_m = 50` and accept that the cached coordinate is sufficient.

---

### 9. Add a Physical Reset/Config Button

Currently there is no way for a clinic technician to:
- Clear a stuck alarm (buzzer won't stop)
- Force a cloud sync retry
- Restart WiFi if they changed the clinic router

**Recommendation:**
- Wire a **momentary push button** to GPIO 0 (boot pin — pressing it during runtime is safe after boot):
```cpp
#define PIN_CONFIG_BTN 0
// In setup: pinMode(PIN_CONFIG_BTN, INPUT_PULLUP);
// In loop:
if (digitalRead(PIN_CONFIG_BTN) == LOW) {
  // Short press (< 2s): silence buzzer, clear local alarms
  // Long press (> 5s): restart ESP32 (triggers WiFi reconnect)
  // Very long press (> 10s): erase WiFi config, enter captive portal
}
```
- This is **essential for clinic staff** who cannot access Serial Monitor.

---

## 🟢 Medium Priority — Quality of Life Improvements

### 10. Replace Hardcoded WiFi Credentials with a Captive Portal

Currently, changing the clinic WiFi password means **reflashing firmware** — impossible to do remotely. For 100 units across Fayoum, this is a maintenance nightmare.

**Recommendation:**
- Use [WiFiManager](https://github.com/tzapu/WiFiManager) library for first-time WiFi setup:
```cpp
#include <WiFiManager.h>
WiFiManager wm;
wm.autoConnect("CoolCycle-Setup");  // Creates setup AP if no WiFi saved
```
- On first boot or after config button long-press: ESP32 broadcasts `CoolCycle-Setup` AP. Tech opens browser, enters clinic WiFi SSID/password. Saved to NVS flash, persists across updates.
- Eliminates the need for any technician to have Arduino IDE.

---

### 11. Predictive Spoilage Alert — "Pre-Warm" Warning

Currently alarms fire **when** temperature goes out of range. For vaccines, the damage is already done. Consider a predictive alert:

**Recommendation:**
- Add a **rate-of-change calculation** in the Rule Engine or firmware:
```cpp
// Track temp change rate
float temp_rate_C_per_min = (temp_internal - prev_temp) / (elapsed_ms / 60000.0f);
// If rising at > 0.5°C/min AND battery < 30%, alert early
if (temp_rate_C_per_min > 0.5 && battery_soc < 30) {
  // Publish "PreWarm" alarm
}
```
- WhatsApp message: *"⚠️ Pre-Warning: Temperature rising at 0.8°C/min. Current: 6.5°C. Estimated breach in ~90 minutes."*
- This gives clinic staff time to **move vaccines before spoilage**, not after.

---

### 12. Add BH1750 Solar Intensity to Remaining Cool Time

Currently solar intensity (lux from BH1750) is logged but not used in the remaining cool time calculation. On a cloudy day, it's the best indicator that solar input will drop.

**Recommendation:**
- Correlate BH1750 lux with INA219 solar power (build a calibration table per panel type).
- In the calculation, use a **rolling 15-minute solar average** rather than instantaneous:
```cpp
// Ring buffer of last 15 solar_power readings (1 per minute)
// Use average to smooth passing clouds
float avg_solar = calcRollingAverage(solar_power_history, 15);
float net_draw_w = max(5.0f, compressor_draw_w - avg_solar);
```

---

### 13. ThingsBoard Dashboard — Add "Inventory Snapshot" as Required by Team Doc

The team task document (`4_6005826084812102891.pdf`) explicitly requires:
> *"Inventory Snapshot: A small checklist showing the quantity of specific vaccines or medicines currently inside."*

This is not currently implemented in the TB dashboard architecture.

**Recommendation:**
- Add a **Markdown/HTML widget** on the clinic staff dashboard showing:
  ```
  💉 OPV        →  120 vials  ✅
  💉 BCG        →   45 vials  ✅
  💉 Hepatitis B →   30 vials  ⚠️ Low
  ```
- Store inventory as shared attributes: `inv_OPV`, `inv_BCG`, etc.
- Allow IT Admin to update counts from the Admin Dashboard (write shared attrs via widget).
- Trigger a `"Low Stock"` alert when any vaccine count drops below a configurable threshold.

---

### 14. Local Dashboard — Add Offline Sync Indicator

When cellular is down, clinic staff see the offline banner, but do not know **how long the data has been stale** or **how many readings are buffered** for cloud sync.

**Recommendation:**
- Add to `/api/status` JSON:
```json
{
  "offline_log_count": 47,
  "offline_since_mins": 120,
  "last_cloud_sync": 1714000000000
}
```
- Show on dashboard: *"📡 Offline for 2h · 47 readings buffered"* when `cloud_online = false`.
- Gives staff confidence the system is still recording even without cloud.

---

### 15. Deploy TB Edge Instead of Direct Cellular for Multi-Unit Clinics

For clinics eventually deploying 3+ CoolCycle units, having each unit maintain its own SIM card and GPRS connection is expensive (~15 EGP × 3 = 45 EGP/month/clinic) and creates 3× failure points.

**Recommendation (Phase 4 consideration):**
- Deploy **ThingsBoard Edge** on a local Raspberry Pi at the clinic.
- All ESP32 units connect via **local WiFi MQTT** to the Edge node.
- The Edge node has a **single SIM card** connecting to ThingsBoard Cloud.
- Benefits:
  - 1 SIM per clinic regardless of number of units (~15 EGP/month flat)
  - Local rule engine runs alarms even if cloud is unreachable
  - Faster local dashboard (no cellular latency)
  - Edge can also serve as local NTP server, solving DS3231 drift

---

## 📊 Business & Operational Recommendations

### 16. Formalize the "Support Call" Button as a Ticketing System

The current WhatsApp "Call for Support" button opens a free-text WhatsApp message. As the fleet grows to 100+ units, technicians will be overwhelmed with unstructured messages.

**Recommendation:**
- Route "Call for Support" through a simple **Typeform or Jotform** that captures: unit ID, issue type, clinic contact, and auto-attaches GPS + current readings via URL parameters.
- Or use **Twilio Studio** to create a structured WhatsApp chatbot flow that triages the issue before connecting to a human.
- Log all support requests to a ThingsBoard asset as a custom telemetry event for tracking mean-time-to-repair (MTTR).

---

### 17. Implement Remote Diagnostics Command

Before dispatching a technician, IT Admin should be able to **remotely query the device state** beyond what telemetry shows.

**Recommendation:**
- Add RPC commands that Admin can trigger from the dashboard:
  | Command | Returns |
  |---------|---------|
  | `getStatus` | Full sensor snapshot + error flags |
  | `testBuzzer` | 3 beeps (confirm speaker works) |
  | `testLED` | 5 flashes (confirm LEDs work) |
  | `scanI2C` | List all I²C addresses found (detect sensor failure) |
  | `getLogCount` | Number of buffered offline readings |
  | `getSignalQuality` | RSSI + GPRS signal strength |
  | `reboot` | Safe restart |
- All already supported by the MQTT RPC handler stub in `coolcycle_phase2_mqtt.ino`.

---

### 18. Assign a Unique QR Code to Each Unit

For a fleet of 100 units, technicians need to rapidly identify which physical unit corresponds to which ThingsBoard device, especially during maintenance.

**Recommendation:**
- Print a QR code on each unit that encodes:
  ```
  https://coolcycle.com/portal/device/CC-0001
  ```
- Scanning opens the Admin Portal pre-filtered to that unit's detail page.
- Use ThingsBoard's device label field to store the QR code content.
- The `DEVICE_ID` constant in firmware (`CC-0001`) should match the physical label and QR code.

---

### 19. Battery Circularity Monitoring — Align with Your Own USP

The Phase 2 submission explicitly states *"Battery circularity: life extended 50%"* as a competitive advantage. Yet there is no telemetry tracking to prove or demonstrate this.

**Recommendation:**
- Track **charge cycle count** in NVS flash (non-volatile, survives reboots):
```cpp
#include <Preferences.h>
Preferences prefs;
prefs.begin("coolcycle", false);
int cycles = prefs.getInt("cycles", 0);
// Increment when SOC crosses from <20% to >80% (one full cycle)
prefs.putInt("cycles", ++cycles);
```
- Publish `charge_cycle_count` as telemetry to ThingsBoard.
- Show on Developer Dashboard: *"Battery has completed 47 cycles (est. 1153 remaining)"*
- This is **direct, measurable proof** of your battery longevity claim — valuable for NGO/MoH reporting.

---

### 20. Plan for Egypt's Summer Heat — The System's Biggest Risk

Fayoum summers reach **45°C ambient** with direct sun. An outdoor-mounted ESP32 enclosure without thermal management can reach 70°C+ inside, which is above the ESP32's rated 85°C but dangerously close given continuous WiFi+SIM800L operation.

**Recommendation:**
- Mount the **electronics enclosure in shade** or inside the insulated cooler cabinet, not in direct sun.
- Use **IP65-rated ABS enclosures** with a small vent + PTFE membrane (waterproof but breathable).
- Add a **thermal threshold alert** using the ESP32's internal temperature sensor:
```cpp
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
float esp32_temp_C = (temprature_sens_read() - 32) / 1.8f;
// Alert if ESP32 internal temp > 75°C
```
- Publish `esp32_temp_c` as telemetry — it's a free sensor you already have.
- Set a MAJOR alarm at 70°C so the maintenance team knows before hardware failure.

---

## Summary Priority Table

| # | Recommendation | Priority | Effort |
|---|---------------|----------|--------|
| 1 | SIM800L dedicated power rail | 🔴 Critical | Low |
| 2 | GPIO 34 hardware pull-up + debounce | 🔴 Critical | Low |
| 3 | DS18B20 waterproof probe + dual placement | 🔴 Critical | Low |
| 4 | Coulomb counting SOC | 🔴 Critical | Medium |
| 5 | MQTT TLS port 8883 | 🟡 High | Medium |
| 6 | Cool time floor + cap at 72h | 🟡 High | Low |
| 7 | Hardware watchdog timer | 🟡 High | Low |
| 8 | GPS fallback + GPS Lost alarm | 🟡 High | Low |
| 9 | Physical reset/config button | 🟡 High | Low |
| 10 | WiFiManager captive portal | 🟡 High | Low |
| 11 | Predictive pre-warm alert | 🟡 High | Medium |
| 12 | Rolling solar average for cool time | 🟢 Medium | Low |
| 13 | Inventory snapshot widget | 🟢 Medium | Medium |
| 14 | Offline sync indicator on dashboard | 🟢 Medium | Low |
| 15 | TB Edge for multi-unit clinics | 🟢 Medium | High |
| 16 | Support ticketing via Twilio Studio | 🟢 Medium | Medium |
| 17 | Remote diagnostics RPC commands | 🟢 Medium | Low |
| 18 | QR code per unit | 🟢 Medium | Low |
| 19 | Charge cycle count telemetry | 🟢 Medium | Low |
| 20 | ESP32 internal temp monitoring + heat alert | 🟢 Medium | Low |

---

> *These recommendations are based on field realities of rural Egyptian clinic deployments,  
> ESP32 hardware constraints, ThingsBoard PE architectural best practices, and alignment  
> with CoolCycle's stated competitive advantages (battery circularity, IoT monitoring, local repair).*
