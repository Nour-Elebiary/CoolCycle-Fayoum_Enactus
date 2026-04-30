# CoolCycle — Deliverable 1: ESP32-DevKit-V1 Pinout & Hardware Integration

> **Role**: Senior IoT & Systems Architect  
> **Target MCU**: DOIT ESP32-DevKit-V1 (30-pin, ESP-WROOM-32 module)  
> **Rule**: All GPIO must be 3.3V logic-safe. ADC1 only when WiFi/cellular active.

---

## 1. Complete GPIO Assignment Table

| GPIO | Pin Label | Sensor / Peripheral | Protocol | Notes / Risk |
|------|-----------|---------------------|----------|--------------|
| **3** | RX0 | — | UART0 | Reserved — USB debug serial |
| **1** | TX0 | — | UART0 | Reserved — USB debug serial |
| **16** | RX2 | Neo-6M GPS RX | UART2 | GPS TX → ESP32 |
| **17** | TX2 | Neo-6M GPS TX | UART2 | ESP32 → GPS RX |
| **25** | GPIO25 | SIM800L/SIM7600 RX | UART1 | Cellular module RX |
| **26** | GPIO26 | SIM800L/SIM7600 TX | UART1 | Cellular module TX |
| **21** | SDA | DS3231 + INA219 #1 + INA219 #2 + BH1750 | I²C | Shared bus, 4.7kΩ pull-up to 3.3V |
| **22** | SCL | DS3231 + INA219 #1 + INA219 #2 + BH1750 | I²C | Shared bus, 4.7kΩ pull-up to 3.3V |
| **4** | GPIO4 | DS18B20 (Internal Probe) | 1-Wire | 4.7kΩ pull-up to 3.3V |
| **13** | GPIO13 | DHT22 (Ambient Temp/Humidity) | Single-Wire | 10kΩ pull-up |
| **34** | GPIO34 | Reed Switch (Door Sensor) | Digital IN | Input-only. MUST add 10kΩ external pull-up |
| **35** | GPIO35 | LDR (Solar Intensity backup) | ADC1 CH7 | ADC1 safe with WiFi; use voltage divider |
| **32** | GPIO32 | Buzzer (Active, 3.3V) | Digital OUT | Drive via 2N2222/BC547 transistor |
| **33** | GPIO33 | LED (Red — Critical Alarm) | Digital OUT | 330Ω series resistor |
| **27** | GPIO27 | SIM800L PWR_KEY | Digital OUT | Active LOW 1s pulse to power on |
| **14** | GPIO14 | SIM800L RST | Digital OUT | Active LOW 100ms |
| **23** | GPIO23 | Sensor Power Gate | Digital OUT | P-channel MOSFET gate for sensor rail |
| **2** | GPIO2 | Status LED (onboard) | Digital OUT | Strapping pin — HIGH on boot OK |

---

## 2. I²C Device Address Map

| Device | Default I²C Address | Function |
|--------|---------------------|----------|
| INA219 #1 (Battery) | `0x40` (A0=GND, A1=GND) | Monitor 4S4P LiFePO4 pack |
| INA219 #2 (Solar Panel) | `0x41` (A0=VCC, A1=GND) | Monitor solar input current/voltage |
| DS3231 RTC | `0x68` | High-accuracy timekeeping |
| BH1750 (Solar Intensity) | `0x23` (ADDR=GND) | Ambient light lux → solar index |

> **No address collision** — all four share the SDA/SCL bus without a multiplexer.

---

## 3. Power Budget Analysis

| Component | Avg Current (mA) | Peak (mA) |
|-----------|-----------------|-----------|
| ESP32 (WiFi active) | 160 | 240 |
| SIM800L (transmit) | 200 | 2000 (burst) |
| DS18B20 | 1.5 | 4 |
| DHT22 | 1.5 | 2.5 |
| INA219 ×2 | 1 | 1 |
| DS3231 | 0.2 | 0.5 |
| BH1750 | 0.12 | 0.19 |
| Neo-6M GPS | 45 | 67 |
| Buzzer | 30 | 40 |
| LED (Red) | 10 | 20 |
| **Total (Active)** | **~450** | **~2375** |

> **Mitigation**: SIM800L burst current (2A) requires a **dedicated 4.0V buck regulator** (e.g., MP1584) fed from the battery, plus a **1000µF bulk capacitor** and 100nF ceramic capacitor directly on the SIM800L VCC line.  
> Add separate 3.3V LDO (AMS1117-3.3 or similar, 1A rated) for ESP32 + sensors.  
> DevKit onboard LDO limited to ~500mA — **do NOT power SIM800L through it**.

---

## 4. 3.3V Logic Safety Rules

| Sensor | Native Voltage | Protection Required |
|--------|---------------|---------------------|
| DS18B20 | 3.3V ✅ | None |
| DHT22 | 3.3V ✅ (can run on 3.3V) | None |
| INA219 | 3.3V ✅ | None |
| DS3231 | 3.3V ✅ | None |
| BH1750 | 3.3V ✅ | None |
| Neo-6M GPS | 3.3V TX ✅ | None |
| SIM800L | 4.0V VCC ⚠️ | Separate 4.0V rail (LiPo cell or buck) |
| SIM7600 | 3.3–4.2V VCC ⚠️ | Level shift UART if 5V variant |
| Reed Switch | 3.3V ✅ | External 10kΩ pull-up |

---

## 5. Boot-Safe Strapping Pin Usage

| GPIO | Role | Strapping Behavior | Safe? |
|------|------|--------------------|-------|
| GPIO0 | Boot mode | HIGH=normal, LOW=flash | ✅ Avoid as sensor pin |
| GPIO2 | Onboard LED | Must be LOW during boot | ✅ Used as status LED (low-risk) |
| GPIO12 | — | HIGH=3.3V flash, LOW=1.8V flash | ⛔ DO NOT pull HIGH |
| GPIO15 | — | Must be HIGH during boot | ✅ Do not connect to GND at boot |

---

## 6. Non-Blocking I/O Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    loop() Scheduler                      │
│                                                          │
│  Timer A: 5000ms  → Read DS18B20, DHT22, BH1750/LDR    │
│  Timer B: 1000ms  → Read INA219 × 2 (power metrics)    │
│  Timer C: 10000ms → Read GPS NMEA sentences             │
│  Timer D: 60000ms → Sync DS3231 timestamp               │
│  Timer E: 500ms   → Check Reed Switch door state        │
│  Timer F: 30000ms → Publish MQTT telemetry batch        │
│  Timer G: 5000ms  → Update local web server JSON        │
│                                                          │
│  ISR:             → Reed Switch interrupt (CHANGE)      │
└─────────────────────────────────────────────────────────┘
```

All timers use `millis()` delta — **zero blocking `delay()` calls** in production firmware.

---

## 7. DS3231 Offline Logging Strategy

When cellular connectivity is lost:
1. ESP32 reads DS3231 for current UTC timestamp.
2. Sensor readings timestamped locally and appended to LittleFS log file (`/log/readings.json`).
3. On reconnect, the firmware reads log file and publishes to ThingsBoard with **backdated timestamps** using the TB MQTT API:
   ```json
   { "ts": 1714000000000, "values": { "temperature": 4.2, "battery_soc": 76 } }
   ```
4. Log file is cleared after successful sync confirmation (MQTT PUBACK received).

