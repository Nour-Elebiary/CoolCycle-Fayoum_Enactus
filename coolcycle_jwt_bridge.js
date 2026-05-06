/**
 * CoolCycle JWT Bridge — coolcycle.com → ThingsBoard PE
 * 
 * Stack: Node.js + Express
 * Deploy: On coolcycle.com backend (or Vercel serverless function)
 * 
 * This bridge:
 * 1. Receives login from coolcycle.com frontend
 * 2. Calls ThingsBoard PE /api/auth/login
 * 3. Returns JWT + redirect URL to customer's dashboard
 * 
 * Install: npm install express node-fetch cors dotenv
 */

require('dotenv').config();
const express = require('express');
const cors    = require('cors');
const fs      = require('fs');
const path    = require('path');

const app = express();
app.use(express.json());
app.use(cors({ origin: 'https://coolcycle.com' }));

const TB_BASE = process.env.TB_BASE_URL || 'https://eu.thingsboard.cloud';
const ASSIGNMENTS_FILE = path.join(__dirname, 'assignments.json');

// ── Helpers ───────────────────────────────────────────────────────────────
function getAssignments() {
  if (!fs.existsSync(ASSIGNMENTS_FILE)) return [];
  try {
    return JSON.parse(fs.readFileSync(ASSIGNMENTS_FILE, 'utf8'));
  } catch (err) {
    return [];
  }
}

function saveAssignments(assignments) {
  fs.writeFileSync(ASSIGNMENTS_FILE, JSON.stringify(assignments, null, 2));
}

// ── Dashboard IDs per role (set these after creating dashboards in TB) ──
const DASHBOARD_IDS = {
  TENANT_ADMIN:    process.env.TB_DASH_DEVELOPER,  // Tier 1
  CUSTOMER_ADMIN:  process.env.TB_DASH_ADMIN,       // Tier 2
  CUSTOMER_USER:   process.env.TB_DASH_USER,        // Tier 3
};

/**
 * POST /api/portal/login
 * Body: { email, password }
 * Returns: { token, refreshToken, dashboardUrl, role }
 */
app.post('/api/portal/login', async (req, res) => {
  const { email, password } = req.body;
  if (!email || !password) {
    return res.status(400).json({ error: 'Email and password required' });
  }

  try {
    // Step 1: Authenticate with ThingsBoard PE
    const tbRes = await fetch(`${TB_BASE}/api/auth/login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: email, password }),
    });

    if (!tbRes.ok) {
      const err = await tbRes.json();
      return res.status(401).json({ error: 'Authentication failed', detail: err.message });
    }

    const { token, refreshToken } = await tbRes.json();

    // Step 2: Get user info to determine role + dashboard
    const userRes = await fetch(`${TB_BASE}/api/auth/user`, {
      headers: { 'X-Authorization': `Bearer ${token}` },
    });
    const user = await userRes.json();
    const authority = user.authority; // TENANT_ADMIN | CUSTOMER_USER

    // Step 3: Determine which dashboard to redirect to
    let dashboardId = DASHBOARD_IDS[authority] || DASHBOARD_IDS['CUSTOMER_USER'];

    // Customer admins vs regular users — check custom attribute
    if (authority === 'CUSTOMER_USER' && user.additionalInfo?.isAdmin) {
      dashboardId = DASHBOARD_IDS['CUSTOMER_ADMIN'];
    }

    const dashboardUrl = `${TB_BASE}/dashboard/${dashboardId}?accessToken=${token}`;

    return res.json({
      token,
      refreshToken,
      dashboardUrl,
      role: authority,
      userEmail: user.email,
      firstName: user.firstName,
    });

  } catch (err) {
    console.error('[CoolCycle Bridge] Error:', err);
    return res.status(500).json({ error: 'Internal server error' });
  }
});

/**
 * POST /api/portal/refresh
 * Body: { refreshToken }
 * Returns: { token, refreshToken }
 */
app.post('/api/portal/refresh', async (req, res) => {
  const { refreshToken } = req.body;
  if (!refreshToken) return res.status(400).json({ error: 'refreshToken required' });

  try {
    const tbRes = await fetch(`${TB_BASE}/api/auth/token`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ refreshToken }),
    });
    const data = await tbRes.json();
    return res.json(data);
  } catch (err) {
    return res.status(500).json({ error: 'Token refresh failed' });
  }
});

/**
 * GET /api/portal/health
 */
app.get('/api/portal/health', (req, res) => {
  res.json({ status: 'ok', tbBase: TB_BASE });
});

// ── Assignments API (ThingsBoard Integration) ─────────────────────────────

/**
 * POST /api/portal/assignments
 * Receives assignment webhook from ThingsBoard Rule Engine
 */
app.post('/api/portal/assignments', (req, res) => {
  const { deviceId, deviceName, customerId, customerName, publicLink } = req.body;
  
  if (!deviceId || !customerId) {
    return res.status(400).json({ error: 'deviceId and customerId required' });
  }

  const assignments = getAssignments();
  const existingIndex = assignments.findIndex(a => a.deviceId === deviceId && a.customerId === customerId);
  
  const assignmentData = {
    deviceId,
    deviceName,
    customerId,
    customerName,
    publicLink: publicLink || '',
    assignedAt: new Date().toISOString()
  };

  if (existingIndex >= 0) {
    assignments[existingIndex] = assignmentData;
  } else {
    assignments.push(assignmentData);
  }

  saveAssignments(assignments);
  console.log(`[CoolCycle Bridge] Saved assignment: ${deviceName} -> ${customerName}`);
  
  return res.json({ status: 'success' });
});

/**
 * GET /api/portal/assignments
 * Returns all assignments (for Admin Portal)
 */
app.get('/api/portal/assignments', (req, res) => {
  res.json(getAssignments());
});

/**
 * GET /api/portal/user-data/:customerId
 * Returns assignments for a specific user
 */
app.get('/api/portal/user-data/:customerId', (req, res) => {
  const assignments = getAssignments();
  const userAssignments = assignments.filter(a => a.customerId === req.params.customerId);
  res.json(userAssignments);
});

const PORT = process.env.PORT || 3001;
app.listen(PORT, () => console.log(`[CoolCycle Bridge] Running on port ${PORT}`));
