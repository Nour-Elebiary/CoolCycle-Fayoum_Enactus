/**
 * CoolCycle MQTT Integration Test
 * 
 * Validates the connection to ThingsBoard using MQTT and sends dummy telemetry.
 * Pre-requisite: npm install mqtt dotenv
 * Run: node tests/mqtt_integration.test.js
 */
const mqtt = require('mqtt');
require('dotenv').config();

// Placeholder for ThingsBoard Host and Token
const TB_HOST = process.env.TB_HOST || 'mqtt://mqtt.thingsboard.cloud';
const TB_TOKEN = process.env.TB_TOKEN || 'DUMMY_TOKEN_FOR_TESTING';

console.log('--- Running MQTT Integration Test ---');
console.log(`Connecting to ${TB_HOST} with token ${TB_TOKEN}...`);

// In a real automated CI pipeline, you would use a dedicated testing endpoint or mocked MQTT broker.
const client = mqtt.connect(TB_HOST, {
    username: TB_TOKEN,
    clientId: 'CoolCycle_Test_Node',
    connectTimeout: 5000,
});

client.on('connect', () => {
    console.log('[TEST] MQTT Connection: => Pass');
    
    // Simulate sending CoolCycle Telemetry
    const telemetryPayload = {
        temp_internal: 4.5,
        battery_soc: 95,
        door_open: false,
        solar_power: 15.2,
        fw_version: "1.0.0-test"
    };

    console.log('[TEST] Publishing telemetry...');
    client.publish('v1/devices/me/telemetry', JSON.stringify(telemetryPayload), (err) => {
        if (err) {
            console.error('[TEST] Publish: => Fail', err);
            client.end();
            process.exit(1);
        } else {
            console.log('[TEST] Publish: => Pass');
            
            // Clean exit
            client.end();
            console.log('--- All tests completed successfully ---');
            process.exit(0);
        }
    });
});

client.on('error', (err) => {
    console.error('[TEST] MQTT Connection: => Fail', err.message);
    client.end();
    process.exit(1);
});
