# CoolCycle — Deliverable 2: ThingsBoard PE Rule Chain Architecture (Consolidated)

> **Platform**: ThingsBoard PE Cloud (Europe Edition)  
> **Rule Engine Version**: 2.0+ (TBEL-first scripting)
> **Plan Constraint**: 3 Rule Chains Max (Free Plan Optimized)

---

## 1. CoolCycle Root Chain (The Traffic Cop)
The primary entry point for all device communication. It handles data persistence and routes messages to specialized handlers.

```
[Input Node]
    ↓
[Message Type Switch]
    ├─ "Post Telemetry"      ──→ [Save Timeseries] ──→ [Rule Chain: CoolCycle Alarms]
    ├─ "Post Attributes"     ──→ [Save Client Attrs]
    ├─ "RPC from Device"     ──→ [Rule Chain: RPC Handler]
    ├─ "Activity Event"      ──→ [Save TS: {online: true}]
    ├─ "Inactivity Event"    ──→ [Save TS: {online: false}]
    │                                ↓
    │                         [Create Alarm: "Device Offline" MAJOR]
    │                                ↓
    │                         [REST API Call: WhatsApp Alert]
    └─ "Entity Assigned"     ──→ [Fetch Public Link] ──→ [Format Payload] ──→ [REST API: Bridge]
```

### TBEL: Format Assignment Payload
```java
// Formats the payload to send to the CoolCycle JWT Bridge
var payload = {
    deviceId: metadata.entityId,
    deviceName: metadata.entityName,
    customerId: metadata.assignedCustomerId,
    customerName: metadata.assignedCustomerName,
    publicLink: metadata.ss_public_dashboard_link != null ? metadata.ss_public_dashboard_link : ''
};
return {msg: payload, metadata: metadata, msgType: msgType};
```

**Telemetry Keys Published by Firmware:**
```json
{
  "ts": 1714000000000,
  "values": {
    "temp_internal":   4.2,
    "temp_ambient":    28.5,
    "humidity":        62.1,
    "battery_voltage": 13.1,
    "battery_current": -2.3,
    "battery_soc":     78,
    "solar_voltage":   17.4,
    "solar_current":   3.8,
    "solar_power":     65.9,
    "solar_lux":       42000,
    "door_open":       false,
    "door_open_secs":  0,
    "latitude":        29.3084,
    "longitude":       30.8428,
    "remaining_cool_hours": 14.2,
    "compressor_on":   true,
    "rssi":            -78,
    "fw_version":      "1.2.3"
  }
}
```

---

## Rule Chain 2: CoolCycle Alarms

```
[Input]
    ↓
[Script Filter: check all alarm conditions — TBEL]
    ↓
┌── Temperature High (CRITICAL > 8°C) ──→ [Create Alarm] ──→ [REST: WhatsApp]
├── Temperature Low  (MAJOR   < 2°C)  ──→ [Create Alarm] ──→ [REST: WhatsApp]
├── Battery Low      (MAJOR   < 20%)  ──→ [Create Alarm] ──→ [REST: SMS]
├── Battery Critical (CRITICAL < 10%) ──→ [Create Alarm] ──→ [REST: WhatsApp]
├── Door Open > 60s  (WARNING)        ──→ [Create Alarm] ──→ [Buzzer RPC]
├── Solar Power = 0  (MINOR)          ──→ [Create Alarm]
├── Device Offline   (MAJOR)          ──→ [Create Alarm] ──→ [REST: WhatsApp]
└── Geofence Exit    (CRITICAL)       ──→ [Create Alarm] ──→ [REST: WhatsApp+GPS]
```

### TBEL: Temperature Alarm Filter
```java
// In Script Filter node — returns true to route to alarm creation
var t = msg.temp_internal;
var vaccineMin = metadata.vaccine_temp_min != null ? 
    parseFloat(metadata.vaccine_temp_min) : 2.0;
var vaccineMax = metadata.vaccine_temp_max != null ? 
    parseFloat(metadata.vaccine_temp_max) : 8.0;

msg.alarm_temp_high = (t > vaccineMax);
msg.alarm_temp_low  = (t < vaccineMin);
msg.alarm_battery   = (msg.battery_soc < 20);
msg.alarm_door      = (msg.door_open_secs > 60);

return true; // always pass through to branching alarm nodes
```

### TBEL: Geofencing Check
```java
// Fetch assigned clinic coords from device shared attributes
var homeLat = parseFloat(metadata.clinic_lat);
var homeLon = parseFloat(metadata.clinic_lon);
var fence_m = parseFloat(metadata.geofence_radius_m != null ? 
    metadata.geofence_radius_m : "500");

var R = 6371000;
var dLat = (msg.latitude - homeLat) * Math.PI / 180;
var dLon = (msg.longitude - homeLon) * Math.PI / 180;
var a = Math.sin(dLat/2) * Math.sin(dLat/2) +
        Math.cos(homeLat * Math.PI/180) * Math.cos(msg.latitude * Math.PI/180) *
        Math.sin(dLon/2) * Math.sin(dLon/2);
var dist = R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));

return dist > fence_m; // true = outside fence → alarm
```

---

## Rule Chain 3: Vaccine Profile Database Sync

This chain handles Customer Admin pushing updated vaccine profiles down to devices via shared attributes.

```
[ATTRIBUTES_UPDATED message — triggered when Admin saves vaccine profile]
    ↓
[Script Filter: msgType == "ATTRIBUTES_UPDATED" && 
                msg contains vaccine_profile keys]
    ↓ True
[Originator Attributes: fetch deviceId, accessToken]
    ↓
[Script Transform: build shared attribute payload]
    ↓
[Save Attributes: SHARED scope]
    ↓
[RPC Call: "pullVaccineProfile" → Device]  ← Device pulls and stores to LittleFS
```

### Shared Attributes Written to Device
```json
{
  "vaccine_name":       "OPV",
  "vaccine_temp_min":   2.0,
  "vaccine_temp_max":   8.0,
  "vaccine_stock":      120,
  "geofence_radius_m":  300,
  "clinic_lat":         29.3084,
  "clinic_lon":         30.8428,
  "alert_whatsapp":     "+201001234567"
}
```

### TBEL: Attribute Sync Transform
```java
// Build the shared attribute update for the device
var out = {};
if (msg.vaccine_name != null) out.vaccine_name = msg.vaccine_name;
if (msg.vaccine_temp_min != null) out.vaccine_temp_min = msg.vaccine_temp_min;
if (msg.vaccine_temp_max != null) out.vaccine_temp_max = msg.vaccine_temp_max;
if (msg.clinic_lat != null) out.clinic_lat = msg.clinic_lat;
if (msg.clinic_lon != null) out.clinic_lon = msg.clinic_lon;
if (msg.geofence_radius_m != null) out.geofence_radius_m = msg.geofence_radius_m;
return {msg: out, metadata: metadata, msgType: "ATTRIBUTES_UPDATED"};
```

---

## Rule Chain 4: WhatsApp / External Alert

```
[Input: Alarm Created message]
    ↓
[Originator Attributes: fetch alert_whatsapp, clinic_name, latitude, longitude]
    ↓
[Script Transform: build WhatsApp message body]
    ↓
[REST API Call Node]
  URL: https://api.twilio.com/2010-04-01/Accounts/{SID}/Messages.json
  Method: POST
  Headers: Authorization: Basic base64(SID:AUTH_TOKEN)
  Body: "From=whatsapp:+14155238886&To=whatsapp:${alert_whatsapp}&Body=${message}"
```

### TBEL: WhatsApp Message Builder
```java
var alarmType = metadata.alarmType;
var clinicName = metadata.clinic_name != null ? metadata.clinic_name : "Unknown Clinic";
var temp = msg.temp_internal != null ? toFixed(msg.temp_internal, 1) : "N/A";
var lat = msg.latitude;
var lon = msg.longitude;
var mapsLink = "https://maps.google.com/?q=" + lat + "," + lon;

var body = "🚨 CoolCycle ALERT\n" +
           "Clinic: " + clinicName + "\n" +
           "Issue: " + alarmType + "\n" +
           "Temp: " + temp + "°C\n" +
           "Battery: " + msg.battery_soc + "%\n" +
           "Location: " + mapsLink + "\n" +
           "Time: " + new Date().toISOString();

msg.whatsapp_body = body;
return {msg: msg, metadata: metadata, msgType: msgType};
```

---

## Rule Chain 5: Predictive Maintenance (Compressor Duty Cycle)

```
[Post Telemetry: compressor_on = true/false]
    ↓
[Calculate Delta Node: compressor_on changes → calculate run duration]
    ↓
[Save Timeseries: compressor_run_minutes, compressor_cycle_count]
    ↓
[Script Filter: compressor_run_minutes > threshold (duty cycle analysis)]
    ↓ True (excessive cycling detected)
[Create Alarm: "Compressor Overload" MAJOR]
    ↓
[REST: WhatsApp to IT Admin]
```

### INA219 Compressor Analysis Logic (TBEL)
```java
// Detect compressor state from battery current draw
// Compressor draws ~8-12A when active on 12.8V LFP pack
var current = Math.abs(msg.battery_current);
msg.compressor_on = (current > 6.0);  // threshold Amps

// Duty cycle: ratio of compressor-on time vs total time
// Calculated on 60-minute rolling window via Originator Telemetry node
var dutyRatio = msg.compressor_on_minutes_1h / 60.0;
msg.compressor_duty_pct = toFixed(dutyRatio * 100, 1);

// Flag if duty cycle > 80% (compressor struggling)
return dutyRatio > 0.80;
```

---

## Rule Chain 6: OTA Firmware Update Trigger

```
[Customer Admin triggers OTA via Dashboard button → RPC]
    ↓
[Scheduler or Manual RPC: "triggerOTA"]
    ↓
[Save Attributes: SHARED]
  fw_title, fw_version, fw_checksum, fw_size, fw_url
    ↓
[RPC Call: "checkFirmware" → Device]
    ↓
Device downloads from fw_url, reports fw_state: DOWNLOADING → UPDATED
```

---

## Alarm Severity Reference Table

| Alarm Type | Severity | Auto-Clear | WhatsApp | Buzzer |
|------------|----------|-----------|----------|--------|
| Temp > Vaccine Max | CRITICAL | Yes (temp ≤ max) | ✅ | ✅ |
| Temp < Vaccine Min | MAJOR | Yes (temp ≥ min) | ✅ | ✅ |
| Battery < 20% | MAJOR | Yes (SOC > 25%) | ✅ | ❌ |
| Battery < 10% | CRITICAL | Yes (SOC > 15%) | ✅ | ✅ |
| Door Open > 60s | WARNING | Yes (door closed) | ❌ | ✅ |
| Device Offline | MAJOR | Yes (reconnects) | ✅ | ❌ |
| Geofence Exit | CRITICAL | Manual | ✅ | ✅ |
| Solar Panel Fault | MINOR | Yes (power > 0) | ❌ | ❌ |
| Compressor Overload | MAJOR | Manual | ✅ | ❌ |

