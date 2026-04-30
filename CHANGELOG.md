# CHANGELOG

All notable changes to the CoolCycle Medical Cold Chain Monitoring project will be documented in this file.

## [1.1.0] - 2026-04-26
### Added
- **Consolidated 3-Chain Architecture**: Optimized for ThingsBoard PE Free Plan (3 rule chain limit).
    - `Root Chain`: Ingestion and routing logic.
    - `Alarms Chain`: Merged Temp/Battery/Geofence alarms + WhatsApp/Twilio alerts + Predictive Maintenance.
    - `RPC Handler`: Merged Vaccine Profile Sync + OTA Update triggers.
- **Predictive Maintenance**: Added "Compressor Overload" detection based on duty cycle analysis (integrated into Alarms chain).
- **Integrated Phase 2 Firmware**: Merged cloud-connectivity (MQTT) and RPC handlers directly into `coolcycle_firmware.ino`.
- **Finalized PE Import Format**: Standardized all rule chain JSONs to index-based array format for guaranteed import success.

### Changed
- Updated `README.md` and `02_rule_chain_logic.md` to reflect the 3-chain consolidation.
- Set production Access Token in `coolcycle_firmware.ino`.

## [Unreleased]

### Added
- Created `docs/ARCHITECTURE.md` with complete Mermaid UML diagrams for Edge-to-Cloud architecture, data sync flow, power tree, and rule chain webhooks.
- Added comprehensive `docs/TESTING_PLAN.md` for hardware validation, cloud integration, and local UI behavior.
- Added `tests/mqtt_integration.test.js` to automatically test connectivity to ThingsBoard via MQTT.
- Added `tests/jwt_bridge.test.js` to run isolated unit tests on the Node.js SSO environment configurations.
- Implemented basic CI/CD pipeline placeholder in `.github/workflows/main.yml` to compile Arduino firmware and run Node.js tests on push.
- Created `docs/SCALABILITY_STRATEGY.md` with expert recommendations on scaling ThingsBoard Microservices and automating X.509 device provisioning.
- Added `CONTRIBUTING.md` and `CODE_OF_CONDUCT.md` to establish open-source guidelines and set up developer environments.

### Improved
- Enhanced code readability and debugging capabilities in `coolcycle_firmware.ino` by injecting `#define DEBUG_MODE` flags, replacing raw `Serial` calls with `DEBUG_PRINT`.
- Standardized file headers and inline comments across `.ino` and `.js` files.
- Improved the provisioning script `coolcycle_provisioning.ino` with clear console outputs strictly defining "Success" and "Failure" criteria.
- Updated `04_offline_dashboard.html` to dynamically calculate visual safe zones (Sparklines) based on database-injected temperature profiles instead of hardcoded 2~8°C ranges.

### Changed
- Updated the ThingsBoard rule chain logic (`rule_chain_alarms.json`) to handle external webhook failures by introducing a 60-second `TbMsgDelayNode` retry mechanism for dropped WhatsApp alerts.

### Security
- Replaced all hardcoded API keys, JWT tokens, Twilio credentials, and MQTT provisioning keys with `<ENCRYPTED_PLACEHOLDER>` values across `.env.example`, `.ino` files, and documentation.
- Appended a dedicated "Security Measures & Risks" section to `README.md` addressing physical hardware tampering (eFuse Secure Boot requirements) and mandatory TLS encryption (Port 8883).

### Next Steps
- Conduct strict laboratory hardware tests utilizing the procedures in `TESTING_PLAN.md`.
- Simulate continuous network/cellular dropouts to validate the `LittleFS` backdate logging reliability.
- Set up an automated Python script for batch factory provisioning using ThingsBoard Device Claiming workflows.
