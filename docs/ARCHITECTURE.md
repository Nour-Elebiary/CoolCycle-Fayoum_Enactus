# CoolCycle System Architecture

This document visually describes the data flows, connectivity, and hardware integration of the CoolCycle Medical Cold Chain Monitoring System.

## High-Level System Architecture

```mermaid
graph TD
    subgraph Edge[Clinic/Field Environment]
        ESP32[ESP32 MCU]
        SENSORS[DS18B20 & DHT22 Temp\nINA219 Power\nGPS & Reed Switch]
        BATTERY[LiFePO4 4S4P Battery]
        SOLAR[Solar Panels]
        
        SENSORS --> ESP32
        BATTERY <--> ESP32
        SOLAR --> ESP32
        
        ESP32 -- "Serves HTML" --> LOCAL_UI[Local Offline Dashboard\n192.168.4.1]
    end

    subgraph Connectivity[Network Layer]
        WIFI[Clinic WiFi]
        GSM[SIM800L GPRS / 2G]
        
        ESP32 -. "Fallback" .-> WIFI
        ESP32 -- "Primary" --> GSM
    end

    subgraph Cloud[ThingsBoard PE Cloud]
        MQTT_GW[MQTT Broker TLS 8883]
        RULE_ENGINE[TB Rule Engine]
        DB[(PostgreSQL / Cassandra)]
        DASHBOARDS[ThingsBoard Dashboards]
        
        GSM -- "MQTT JSON" --> MQTT_GW
        MQTT_GW --> RULE_ENGINE
        RULE_ENGINE --> DB
        DB --> DASHBOARDS
    end

    subgraph External[External Services]
        TWILIO[Twilio API]
        WHATSAPP[WhatsApp User]
        PORTAL[Node.js Admin Portal]
        
        RULE_ENGINE -- "REST Webhook" --> TWILIO
        TWILIO -- "Message" --> WHATSAPP
        PORTAL -- "JWT Auth" --> MQTT_GW
    end
```

## Offline-to-Online Telemetry Sync Flow

```mermaid
sequenceDiagram
    participant S as Sensors
    participant E as ESP32 (Firmware)
    participant L as LittleFS (Storage)
    participant M as SIM800L (Modem)
    participant T as ThingsBoard Cloud

    loop Every 5 Seconds
        S->>E: Read Temp, Power, GPS
    end

    loop Every 60 Seconds
        E->>E: Check Cellular Connection
        alt Cellular Offline
            E->>L: Append payload to /log/ offline buffer
            E-->>E: Update Local UI "Offline Sync Count"
        else Cellular Online
            alt Logs Buffered in LittleFS
                L->>E: Read oldest JSON payload
                E->>T: MQTT Publish (with historical timestamp)
                T-->>E: MQTT ACK
                E->>L: Delete payload from buffer
            else No Logs Buffered
                E->>T: MQTT Publish (live telemetry)
            end
        end
    end
```

## Hardware Component Power Tree

```mermaid
flowchart LR
    SOLAR[Solar Panel 18V] --> BUCK1[MPPT Charge Controller]
    BUCK1 --> BATT[LiFePO4 Battery 12.8V]
    
    BATT --> COMP[Cooler Compressor 12V]
    BATT --> BUCK2[Buck Converter 5V]
    BATT --> BUCK3[Buck Converter 4.0V]
    
    BUCK2 --> ESP32[ESP32 VIN 5V]
    BUCK3 --> SIM[SIM800L VCC 4.0V Peak 2A]
    
    ESP32 --> LDO[Internal 3.3V LDO]
    LDO --> SENSORS[I2C & DS18B20 Sensors 3.3V]
```

## Twilio Webhook Retry Logic (Rule Chain)

```mermaid
stateDiagram-v2
    [*] --> CreateAlarm: Temp > Threshold
    CreateAlarm --> REST_Call: Trigger WhatsApp Webhook
    
    REST_Call --> Success: 200 OK
    REST_Call --> Failure: Timeout or 5xx Error
    
    Failure --> DelayNode: Wait 60 Seconds
    DelayNode --> REST_Call: Retry Request
    
    Success --> [*]
```
