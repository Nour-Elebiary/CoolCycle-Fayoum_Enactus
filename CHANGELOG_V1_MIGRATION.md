# CoolCycle Migration Log: ESP32-DevKit-V4 to V1 (30-Pin)

This log records the complete refactoring of the CoolCycle IoT system to support the **DOIT ESP32-DevKit-V1** (30-pin) board, resolving hardware conflicts and ensuring 100% architectural alignment.

**Migration Date:** April 30, 2026  
**Target Hardware:** DOIT ESP32-DevKit-V1 (ESP-WROOM-32)

---

## 1. Global GPIO Remapping
To avoid strapping pin issues and ADC2/WiFi conflicts, the following pin map was implemented:

| Peripheral | V4 (Old) | V1 (New) | Reason |
|------------|----------|----------|--------|
| **DHT22** | GPIO 5 | **GPIO 13** | Avoid GPIO 5 (Strapping pin) |
| **Buzzer** | GPIO 25 | **GPIO 32** | Avoid ADC2 (Fails with WiFi active) |
| **Alarm LED** | GPIO 26 | **GPIO 33** | Avoid ADC2 (Fails with WiFi active) |
| **SIM800L RX/TX** | UART2 (16/17) | **UART1 (25/26)** | Swapped to free UART2 for GPS |
| **GPS RX/TX** | UART1 (18/19) | **UART2 (16/17)** | Avoid SPI pins (18/19) and use HW UART2 |
| **SIM PWR_KEY** | GPIO 23 | **GPIO 27** | Freed GPIO 23 for Sensor Gate |
| **SIM RST** | - | **GPIO 14** | Added hardware reset capability |
| **Sensor Power** | - | **GPIO 23** | Added power gating for deep sleep |
| **Status LED** | GPIO 27 | *Removed* | Simplified layout to single alarm LED |

---

## 2. Firmware Changes

### `coolcycle_firmware.ino`
- Updated board header and definitions.
- Corrected `gpsSerial(2)` initialization.
- Added `#define PHASE2_ENABLED` guard to allow seamless integration with the MQTT module.
- Implemented `setupPhase2()` and `loopPhase2()` hooks.
- Added `PIN_SENSOR_PWR` logic to power-gate the sensor rail during boot.

### `coolcycle_phase2_mqtt.ino`
- Updated `modemSerial(1)` to use UART1 (25, 26).
- Integrated `PIN_SIM_RST` (14) into the `initModem()` sequence with a hardware reset pulse.
- Updated `PWR_KEY` to GPIO 27.
- Wrapped in functions `setupPhase2()` and `loopPhase2()` for better main-sketch integration.

### `coolcycle_provisioning.ino`
- Updated LED pin to 33.
- Replaced Green LED success indicator with a 6-beep sequence on the Buzzer (32).

---

## 3. Documentation Updates

### `01_pinout_and_hardware.md`
- Completely overhauled the GPIO Assignment Table.
- Updated strapping pin warnings (removed GPIO 5).
- Corrected UART Protocol mappings.

### `05_implementation_roadmap.md`
- Replaced all V4 board references with V1.
- Updated Phase 1 test criteria to remove Green LED.
- Updated Phase 2 cellular code stubs to use UART1 (Serial1).

### `06_services_setup_guide.md`
- Updated Step 3.5 to reflect the Buzzer success signal during provisioning.
- Verified all manual setup steps remain valid for the new pinout.

### `README.md` & `CONTRIBUTING.md`
- Updated MCU and module specifications to `DOIT ESP32-DevKit-V1`.
- Synchronized the GPIO Quick Reference table.

---

## 4. Build Environment
- Created `platformio.ini` specifically configured for `board = esp32doit-devkit-v1`.
- Added `custom_partitions.csv` to ensure 4MB flash is partitioned correctly for LittleFS and OTA updates.

---

**Status:** ALL CHANGES VERIFIED AND ALIGNED.
**Architect:** Antigravity (Advanced Agentic Coding AI)
