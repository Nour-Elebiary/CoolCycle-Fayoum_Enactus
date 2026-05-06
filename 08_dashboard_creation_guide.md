# CoolCycle — Dashboard Creation & Configuration Guide

This guide provides a comprehensive, step-by-step walkthrough for creating, configuring, and assigning Dashboards in ThingsBoard PE for the CoolCycle platform.

---

## The "Why" and "When": Dashboard Tiers

ThingsBoard utilizes a strict hierarchy. Dashboards must be designed with the target audience's permissions in mind.

| Tier | Target Audience | When to use? | Why? |
|------|-----------------|--------------|------|
| **Tenant (Developer)** | You (CoolCycle Engineers) | Fleet monitoring, system debugging, global OTA updates. | Needs full access to *all* devices across *all* clinics. Unrestricted read/write. |
| **Customer Admin** | Clinic IT / Managers | Managing their specific clinic's devices, setting up vaccine profiles. | They need to manage their own fleet but must be strictly isolated from other clinics. |
| **Customer User** | Nurses / End-Users | Daily monitoring of a specific fridge via the CoolCycle Website. | Needs a dead-simple, read-only view. No configuration, just status and alerts. |

---

## 1. Creating the Tenant (Developer) Dashboard

**Goal:** A "God Mode" view of the entire CoolCycle fleet.

### Role Restrictions & Responsibilities
- **Responsibilities**: Fleet-wide monitoring, firmware management (OTA), root cause analysis, provisioning new devices.
- **Permissions**: `ALL` access.
- **Restrictions**: None. Developers can see all data across all clinics.

### Required Feature Widgets
- **Global Map Widget**: To see the geographic distribution of all devices.
- **OTA Update Button**: An `RPC Button` widget configured to send the `triggerOTA` command to a device.
- **System Health Gauges**: Monitoring Solar power, battery health across the fleet.
- **Rule Engine / Alarm Tables**: To debug system-wide routing issues.

### Step 1: Initialize
1. Log in to ThingsBoard as **Tenant Administrator**.
2. Navigate to **Dashboards** (left menu) → Click the **`+`** icon → **Create new dashboard**.
3. Name it: `[DEV] CoolCycle Fleet Command Center`.
4. Leave it assigned to the Tenant (do not assign to a customer yet).

### Step 2: Configure Aliases (The Data Binding)
*Aliases tell the dashboard what data to fetch.*
1. Click **Edit Mode** (pencil icon bottom right).
2. Click **Entity Aliases** (icon at top right).
3. Add Alias:
   - **Alias Name:** `All CoolCycle Units`
   - **Filter Type:** `Device Type`
   - **Device Type:** `coolcycle-unit`
4. Save the alias.

### Step 3: Add Widgets (The UI)
1. Click **Add new widget**.
2. **Map Widget:** 
   - Select `Maps` bundle → `OpenStreetMap`. 
   - Datasource: Entity alias `All CoolCycle Units`.
   - Data keys: `latitude`, `longitude`, `temp_internal`.
3. **Alarms Widget:**
   - Select `Alarm widgets` bundle → `Alarms table`.
   - Datasource: Entity alias `All CoolCycle Units`.
4. **Save** the dashboard (Checkmark icon bottom right).

---

## 2. Creating the Customer Admin Dashboard

**Goal:** A management console scoped *only* to the devices owned by a specific clinic.

### Role Restrictions & Responsibilities
- **Responsibilities**: Overseeing the clinic's specific inventory, updating vaccine temperature thresholds, monitoring stock levels, and responding to localized alarms.
- **Permissions**: `READ` telemetry, `READ/WRITE` shared attributes, `ALARM_ACK`.
- **Restrictions**: **Strictly isolated.** Cannot view devices assigned to other clinics. Cannot access the Rule Engine. Cannot trigger OTA firmware updates (to prevent breaking critical medical hardware).

### Required Feature Widgets
- **Vaccine Profile Editor**: Use `Input Widgets` (String Input, Number Input) bound to device Shared Attributes (`vaccine_name`, `vaccine_temp_min`, etc.) to let the Admin update cooling parameters.
- **Action Button**: An `RPC Button` labeled "Sync to Device" that triggers the device to pull the new profile.
- **Scoped Map**: An OpenStreetMap widget that *only* shows devices in their clinic.
- **Historical Trends**: `Timeseries Line Charts` showing temperature history for auditing purposes.

### Step 1: Initialize
1. As Tenant Admin, go to **Dashboards** → **Create new dashboard**.
2. Name it: `[ADMIN] Customer Admin Dashboard`.

### Step 2: Configure Customer-Scoped Aliases
*Crucial Step: You must use the "Customer" filter so the dashboard dynamically shows only the logged-in customer's devices.*
1. Enter **Edit Mode** → **Entity Aliases**.
2. Add Alias:
   - **Alias Name:** `My Clinic Devices`
   - **Filter Type:** `Device Type` (or `Entity Group` if using PE features).
   - **Device Type:** `coolcycle-unit`
   - *ThingsBoard automatically scopes this to the viewing customer when they log in.*

### Step 3: Add Widgets & States
1. Add a **Devices Table** widget (from `Cards` bundle) bound to `My Clinic Devices`.
2. **Dashboard States (Drill-down):**
   - Click **Manage Dashboard States** (top right).
   - Add a new state called `device_details`.
   - Go back to your Devices Table widget settings → **Actions** tab.
   - Add action: `On row click` → Type: `Navigate to new dashboard state` → Target: `device_details`.
3. Switch to the `device_details` state and add specific widgets (Temperature Gauge, Battery Bar) bound to the specific device clicked.
4. **Save** the dashboard.

### Step 4: Assign to Customer
1. Go to **Customers** → Select the clinic (e.g., "El Nazla Health Unit").
2. Click **Manage Dashboards** (dashboard icon in customer details).
3. Click `+` and assign `[ADMIN] Customer Admin Dashboard`.

---

## 3. Creating the Customer User (Public) Dashboard

**Goal:** A read-only, embedded iframe for the CoolCycle website.

### Role Restrictions & Responsibilities
- **Responsibilities**: Immediate, daily monitoring of a specific fridge by non-technical staff (nurses).
- **Permissions**: `READ` telemetry only.
- **Restrictions**: **Strictly Read-Only.** Cannot alter vaccine profiles, cannot acknowledge system alarms, cannot change alert phone numbers. 

### Required Feature Widgets
- **Temperature Status**: `Radial Gauge` colored green for safe ranges (e.g., 2-8°C) and red for danger zones.
- **Battery & Solar Status**: `Linear Gauge` for Battery SOC % and `Value Card` for Solar Power status.
- **Door Status**: `LED Indicator` (Green = Closed, Red = Open).
- **No Input Widgets**: Absolutely no form fields or RPC buttons should exist on this dashboard to prevent accidental configuration changes by medical staff.

### Step 1: Initialize
1. Create a new dashboard named `[USER] Clinic Staff Monitor`.
2. Check the box for **"Display dashboard toolbar" -> Off** (in dashboard settings). This makes it look like a native part of your website when embedded.

### Step 2: Single-Device Alias
1. Enter **Edit Mode** → **Entity Aliases**.
2. Add Alias:
   - **Alias Name:** `Assigned Unit`
   - **Filter Type:** `Single Entity` (Select the specific device, e.g., CC-0001).
   - *Note: For highly dynamic setups, use "Current Customer" relations, but "Single Entity" is safest for individual public links.*

### Step 3: Add Read-Only Widgets
1. Add **Radial Gauge** (Charts bundle) for `temp_internal`.
2. Add **Linear Gauge** for `battery_soc`.
3. Add a **Value Card** for `remaining_cool_hours`.
4. *Do not add any RPC buttons or input forms.*
5. **Save** the dashboard.

### Step 4: Make Public & Integrate
*This connects with the automated integration we built.*
1. Go to **Dashboards** → Find `[USER] Clinic Staff Monitor`.
2. Click the **Make Public** icon (Globe icon).
3. *Optional*: Run the `setup_public_dashboard.js` script to automatically link this dashboard's public URL to the specific customer in the backend.

---

## Best Practices & Troubleshooting

- **Alias Errors?** If widgets say "No Data", you likely chose `Single Entity` for an alias but the logged-in user doesn't have permission to see that entity. Always try to use `Device Type` or `Relations` aliases for Customer dashboards.
- **Mobile Responsiveness:** ThingsBoard dashboards are responsive, but you should manually test them. In Edit Mode, resize your browser window and adjust widget widths/heights to ensure they stack correctly on phones.
- **Color Themes:** Go to Dashboard Settings (gear icon in Edit Mode) to change the theme to "Dark" or inject custom CSS to match the CoolCycle brand colors (`#00d4aa`).
