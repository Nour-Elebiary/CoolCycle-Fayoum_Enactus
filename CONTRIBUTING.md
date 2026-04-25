# Contributing to CoolCycle IoT Platform

Thank you for your interest in contributing to the CoolCycle open-source medical cold chain project. Our goal is to provide reliable, scalable, and secure off-grid refrigeration monitoring for rural clinics.

## Getting Started

### 1. Hardware Setup
To test firmware changes, you will need:
- ESP32-DevKit-V4 (WROOM-32D)
- SIM800L module (with a dedicated 4.0V buck regulator)
- DS18B20 waterproof temperature probe
- INA219 current sensors (x2)
- Neo-6M GPS Module
- See `docs/ARCHITECTURE.md` and `01_pinout_and_hardware.md` for exact wiring diagrams.

### 2. Software Development Environment
#### Firmware (Arduino IDE / PlatformIO)
1. Install [Arduino IDE](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/).
2. Add the ESP32 board manager URL: `https://dl.espressif.com/dl/package_esp32_index.json`.
3. Install the required libraries via Library Manager:
   - `DallasTemperature`
   - `Adafruit INA219`
   - `RTClib`
   - `TinyGPSPlus`
   - `TinyGSM`
   - `PubSubClient`
   - `ArduinoJson`
   - `ESPAsyncWebServer`

#### Backend & Dashboard (Node.js)
1. Install [Node.js](https://nodejs.org/en/) (v18+).
2. Run `npm install` in the `architecture/` directory to install dependencies.
3. Copy `.env.example` to `.env` and fill in your test ThingsBoard credentials.
4. Run `npm run dev` to start the JWT bridge locally.

## Development Workflow

1. **Fork & Branch**: Create a new branch for your feature (`feature/add-new-sensor`) or bugfix (`bugfix/fix-wdt-reboot`).
2. **Code Standards**:
   - Write meaningful inline comments.
   - Use the `DEBUG_PRINT()` macro instead of `Serial.print()` for logging to support production toggle.
3. **Run Tests**:
   - Run `npm test` to execute the Node.js MQTT integration and JWT unit tests.
4. **Submit a Pull Request**: Provide a clear description of the problem solved and link any relevant GitHub Issues.

## Code of Conduct
Please read and adhere to our [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Submitting Bugs
If you find a bug, please create an Issue with:
- The firmware version.
- The exact hardware configuration.
- Steps to reproduce the error.
- Serial Monitor output (with `#define DEBUG_MODE 1` enabled).
