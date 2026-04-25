# CoolCycle Testing Plan

This document outlines the exact procedures required to validate the CoolCycle hardware, firmware, and cloud infrastructure prior to field deployment.

## 1. Hardware & Sensor Validation

### 1.1 Temperature Accuracy
- **Setup**: Place the waterproof DS18B20 probe and the DHT22 in an ice bath alongside a calibrated reference thermometer.
- **Expected**: `temp_internal` (DS18B20) reads 0°C ±0.5°C.
- **Pass/Fail Criteria**: The ESP32 logs the correct temperature. If the probe is completely disconnected, the firmware must default to `-127.0` and immediately trigger a sensor fault warning.

### 1.2 Battery Coulomb Counting
- **Setup**: Use an adjustable DC load tester on the battery output while the INA219 monitors current. Set the load to draw exactly 2.0A.
- **Expected**: The `battery_soc` percentage should decrease linearly over 1 hour, exactly matching the 2Ah capacity drained.
- **Pass/Fail Criteria**: `battery_charge_Ah` accurately tracks drain. Connect the solar panel to verify that `battery_charge_Ah` increases during charging and resets to 100% when current tapers.

### 1.3 Hardware Watchdog (WDT)
- **Setup**: Intentionally introduce an infinite `while(1)` loop inside the `loopPhase2()` function.
- **Expected**: The ESP32 must reboot itself automatically after exactly 60 seconds of unresponsiveness.
- **Pass/Fail Criteria**: The red LED flashes on boot, indicating a successful Watchdog reset.

## 2. Cloud & Telemetry Verification

### 2.1 Webhook & Network Drop Simulation
- **Setup**: Disconnect the cellular antenna from the SIM800L module. Trigger a High Temperature alarm by placing the DS18B20 in warm water.
- **Expected**: The firmware writes the alarm payload to LittleFS `/log/`.
- **Pass/Fail Criteria**: Reconnect the antenna. The device must automatically push the buffered payload to ThingsBoard, and the WhatsApp Rule Chain must retry and deliver the alert.

### 2.2 Integration Tests (Automated)
- **Setup**: Run `npm test` inside the architecture folder.
- **Expected**: The Node.js MQTT Integration test connects to ThingsBoard and successfully publishes a mock payload. The JWT Bridge unit tests validate environment mappings.

## 3. UI & Dashboard Testing

### 3.1 Local Offline Dashboard (`04_offline_dashboard.html`)
- **Setup**: Connect a mobile phone to the `CoolCycle-CC0001` WiFi network.
- **Expected**: Navigating to `http://192.168.4.1` loads the dashboard.
- **Pass/Fail Criteria**: Disconnect the ESP32 from the internet. The dashboard must prominently display the "Cloud Offline — buffered locally" banner, and the sparkline chart must dynamically resize and update every 5 seconds.

### 3.2 Admin Portal Provisioning
- **Setup**: Run `node coolcycle_jwt_bridge.js` locally. Open `coolcycle_admin_portal.html` and push a new vaccine profile (e.g., Moderna: -25°C to -15°C).
- **Expected**: The new profile is synced to the device.
- **Pass/Fail Criteria**: The device buzzer triggers if the current temperature is outside the *new* -25°C to -15°C range, proving dynamic threshold syncing.
