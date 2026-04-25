# CoolCycle — Deliverable 3: ThingsBoard PE Dashboard Architecture

> **Platform**: ThingsBoard PE with Advanced RBAC  
> **Edition**: Europe Cloud (thingsboard.cloud/eu)

---

## Entity Model

```
Tenant: CoolCycle (Developer)
│
├── Asset Profile: "clinic"
│     Attributes: clinic_name, clinic_gov, clinic_lat, clinic_lon, contact_phone
│
├── Device Profile: "coolcycle-unit"
│     Transport: MQTT (cellular)
│     Queue: HighPriority
│     Alarm Rules: (temp, battery, door, offline)
│     OTA: fw_title / fw_version
│
├── Asset: "Fayoum Region"        [type: region]
│   └── Contains → Asset: "El Nazla Clinic"   [type: clinic]
│       └── Contains → Device: "CC-0001"       [profile: coolcycle-unit]
│
├── Customer: "El Nazla Health Unit"
│   ├── Assigned Devices: CC-0001
│   └── Assigned Dashboards: Admin Dashboard, End-User Dashboard
│
└── Dashboards:
    ├── [DEV]   CoolCycle Fleet Command Center
    ├── [ADMIN] Customer Admin Dashboard
    └── [USER]  Clinic Staff Monitor
```

---

## Tier 1: Developer Dashboard — Fleet Command Center

**Access**: CoolCycle Tenant Admin only  
**Purpose**: Full fleet oversight, rule engine, provisioning

### Dashboard States

#### State 1: Fleet Overview (Root)
| Widget | Type | Alias | Keys |
|--------|------|-------|------|
| Fleet Map | OpenStreetMap | Device type: coolcycle-unit | latitude, longitude, status |
| KPI — Total Units | Value Card | Device type | count |
| KPI — Active Units | Value Card | Filter: online=true | count |
| KPI — Units at Risk | Value Card | Filter: alarm active | count |
| KPI — Avg Temp Fleet | Value Card | Device type | temp_internal AVG |
| Active Alarms Table | Alarms Widget | All devices | type, severity, time |
| Devices Table | Entities Table | Device type | name, temp, battery, status |

#### State 2: Unit Detail (Drill-down on map marker click)
| Widget | Type | Data Key |
|--------|------|----------|
| Temperature Gauge | Radial Gauge | temp_internal (2–8°C range, color zones) |
| Ambient Temp/Humidity | Card | temp_ambient, humidity |
| Battery SOC | Linear Gauge | battery_soc |
| Solar Power Flow | Value Card | solar_power, solar_voltage |
| INA219 Power Chart | Time-Series | battery_current, solar_current (24h) |
| Door Status | LED Indicator | door_open |
| GPS Map | OpenStreetMap | latitude, longitude |
| Compressor Duty | Doughnut Chart | compressor_duty_pct (24h) |
| Remaining Cool Time | Value Card | remaining_cool_hours |
| Alarms History | Alarm Table | All for this device |
| OTA Trigger Button | RPC Button | method: triggerOTA |

#### State 3: Vaccine Profile Editor (Developer config)
| Widget | Type | Purpose |
|--------|------|---------|
| Vaccine Name Input | String Input | Write shared attr: vaccine_name |
| Min Temp Slider | Slider Input | Write shared attr: vaccine_temp_min |
| Max Temp Slider | Slider Input | Write shared attr: vaccine_temp_max |
| Stock Count | Number Input | Write shared attr: vaccine_stock |
| Geofence Radius | Number Input | Write shared attr: geofence_radius_m |
| Clinic GPS Setter | Map Click | Write shared attrs: clinic_lat, clinic_lon |
| Save Button | RPC Button | Triggers rule chain attribute sync |

---

## Tier 2: Customer Admin Dashboard (IT Staff)

**RBAC Role**: Group Role — scoped to Customer's device group  
**Permissions**: READ, READ_TELEMETRY, READ_ATTRIBUTES, WRITE_ATTRIBUTES, RPC_CALL, ALARM_ACK  
**Cannot**: Access Rule Engine, other customers' devices, global provisioning

### Dashboard Tabs

#### Tab 1: My Fleet Overview
- Map of all assigned CoolCycle units (clinic locations)
- Summary cards: Active / Warning / Offline counts
- Temperature trend chart (all units, last 24h)

#### Tab 2: Unit Detail (same as Dev Tier 2 but no OTA trigger)
- Real-time gauges, power flow, door status, alarms

#### Tab 3: Vaccine Profile Manager (CRUD)
| Widget | Operation |
|--------|-----------|
| Vaccine Profile Table | List saved profiles from Asset attributes |
| Add Vaccine Profile | Form → WRITE shared attributes to device |
| Edit Profile | Row click → Edit form → WRITE |
| Delete Profile | Row action → Clear shared attribute keys |
| "Push to Device" Button | RPC: pullVaccineProfile → device pulls LittleFS update |

#### Tab 4: Reports
- Daily/Weekly PDF export (PE feature)
- Temperature compliance report (time inside 2–8°C %)
- Door opening count log

#### Tab 5: Alerts Config
- Set WhatsApp number for alerts (WRITE shared attr: alert_whatsapp)
- Enable/disable specific alarm types

---

## Tier 3: End-User Dashboard (Clinic Staff) — Read-Only

**RBAC Role**: Generic Role — READ, READ_TELEMETRY only  
**Purpose**: Operational awareness for non-technical clinic staff

### Layout (Single State, No Navigation Required)

```
┌────────────────────────────────────────────────┐
│           CoolCycle — El Nazla Clinic           │
├──────────────┬─────────────────────────────────┤
│              │     🌡️ Internal Temp             │
│  STATUS ICON │     ████████░░░░  4.2°C ✅      │
│  🟢 SAFE     ├─────────────────────────────────┤
│              │     🔋 Battery Status             │
│              │     ████████████░  78% ✅        │
├──────────────┴─────────────────────────────────┤
│  ⏱ Remaining Cool Time:  14.2 hrs              │
├────────────────────────────────────────────────┤
│  🚪 Door:  CLOSED ✅   │  ☀️ Solar: CHARGING   │
├────────────────────────────────────────────────┤
│  💉 Vaccine: OPV   Stock: 120 vials            │
│  Safe Range: 2°C – 8°C                         │
├────────────────────────────────────────────────┤
│  📊 Temperature (Last 24 Hours)                │
│  [Sparkline chart widget]                       │
├────────────────────────────────────────────────┤
│  🆘 Call for Support (WhatsApp + GPS)          │
│  [Button widget — opens WhatsApp link]         │
└────────────────────────────────────────────────┘
```

### End-User Widget Specs
| Widget | Type | Key | Alarm Color |
|--------|------|-----|-------------|
| Temp Gauge | Radial Gauge | temp_internal | Green 2-8°C / Red outside |
| Battery Bar | Linear Gauge | battery_soc | Green >30% / Yellow >15% / Red <15% |
| Door Icon | LED Indicator | door_open | Green=closed / Red=open |
| Cool Time | Value Card | remaining_cool_hours | Green >4h / Yellow >2h / Red <2h |
| Solar Status | Value Card | solar_power | Green >0 / Grey=0 |
| Vaccine Info | HTML Card | vaccine_name, vaccine_stock | — |
| Temp History | Sparkline | temp_internal (24h) | — |
| Support Button | HTML Widget | Opens `wa.me/` deep link | — |

---

## RBAC Configuration Summary

```
User Group: "CoolCycle Developers"
  Role: Generic → ALL permissions on ALL entity types
  
User Group: "IT Admin — El Nazla"
  Role: Group → Device Group "El Nazla Devices"
    Permissions: READ, READ_TELEMETRY, WRITE_ATTRIBUTES, RPC_CALL, ALARM_ACK
  Default Dashboard: Customer Admin Dashboard

User Group: "Clinic Staff — El Nazla"
  Role: Group → Device Group "El Nazla Devices"
    Permissions: READ, READ_TELEMETRY, READ_ATTRIBUTES
  Default Dashboard: End-User Clinic Monitor
  Always Fullscreen: Yes
  Toolbar: Hidden
```

---

## coolcycle.com → ThingsBoard JWT/SSO Bridge

### Flow
```
[User logs in at coolcycle.com]
    ↓
[Backend call: POST /api/auth/login]
  Body: {"username": "user@clinic.eg", "password": "..."}
  Response: {"token": "JWT...", "refreshToken": "..."}
    ↓
[Redirect to ThingsBoard with token]
  URL: https://eu.thingsboard.cloud/dashboards/{dashboardId}?accessToken={JWT}
    ↓
[ThingsBoard validates JWT → opens assigned dashboard]
[User sees their tier dashboard — no second login]
```

### REST API for Login
```
POST https://eu.thingsboard.cloud/api/auth/login
Content-Type: application/json
{
  "username": "clinic.staff@elnazla.eg",
  "password": "securepassword"
}

Response:
{
  "token": "eyJhbGciOiJSUzUxMiJ9...",
  "refreshToken": "eyJhbGciOiJSUzUxMiJ9..."
}
```

Use the `token` as `X-Authorization: Bearer {token}` for all subsequent API calls.

