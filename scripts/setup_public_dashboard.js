require('dotenv').config({ path: '../.env' });
const fetch = require('node-fetch');

const TB_BASE = process.env.TB_BASE_URL || 'https://eu.thingsboard.cloud';
const TB_USER = process.env.TB_TENANT_EMAIL || 'admin@coolcycle.com'; // Replace with real admin email
const TB_PASS = process.env.TB_TENANT_PASS || 'password'; // Replace with real admin password

async function run() {
  const customerId = process.argv[2];
  const dashboardId = process.argv[3];

  if (!customerId || !dashboardId) {
    console.log('Usage: node setup_public_dashboard.js <CUSTOMER_ID> <DASHBOARD_ID>');
    console.log('Automates making a dashboard public and assigning its link to a customer.');
    process.exit(1);
  }

  try {
    // 1. Login
    console.log(`Logging into ${TB_BASE}...`);
    const loginRes = await fetch(`${TB_BASE}/api/auth/login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: TB_USER, password: TB_PASS })
    });
    if (!loginRes.ok) throw new Error('Login failed. Check credentials.');
    const { token } = await loginRes.json();
    const headers = { 'X-Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' };

    // 2. Fetch Public Customer ID
    const publicUserRes = await fetch(`${TB_BASE}/api/tenant/customers?pageSize=10&page=0&textSearch=Public`, { headers });
    const publicUsers = await publicUserRes.json();
    const publicCustomer = publicUsers.data.find(c => c.title === 'Public');
    
    if (!publicCustomer) throw new Error('Public Customer not found.');
    const publicCustomerId = publicCustomer.id.id;

    // 3. Make Dashboard Public (Assign to Public Customer)
    console.log('Making dashboard public...');
    await fetch(`${TB_BASE}/api/customer/${publicCustomerId}/dashboard/${dashboardId}`, {
      method: 'POST',
      headers
    });

    // 4. Construct Public Link
    const publicLink = `${TB_BASE}/dashboard/${dashboardId}?publicId=${publicCustomerId}`;
    console.log(`Generated Public Link: ${publicLink}`);

    // 5. Save as Server Attribute to the target Customer
    console.log(`Saving 'public_dashboard_link' to Customer ${customerId}...`);
    const attrRes = await fetch(`${TB_BASE}/api/plugins/telemetry/CUSTOMER/${customerId}/SERVER_SCOPE`, {
      method: 'POST',
      headers,
      body: JSON.stringify({ public_dashboard_link: publicLink })
    });

    if (attrRes.ok) {
      console.log('✅ Success! The integration is fully set up for this customer.');
    } else {
      throw new Error(`Failed to save attribute: ${attrRes.status}`);
    }

  } catch (e) {
    console.error('❌ Error:', e.message);
  }
}

run();
