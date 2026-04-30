# Scalability Strategy & DevOps Memo

## 1. ThingsBoard Cluster Architecture (Microservices)

The current architecture utilizes ThingsBoard Professional Edition (PE) running as a monolithic instance or on the managed EU cloud. To scale beyond 10,000 devices with high-frequency MQTT telemetry, the infrastructure must transition to a **ThingsBoard Microservices Architecture (MSA)**.

### Recommended MSA Components
- **TB-Core**: Handles REST API calls, WebSocket subscriptions, and device provisioning. Scale horizontally behind an HAProxy or NGINX load balancer.
- **TB-Rule-Engine**: Offloads the rule chain execution. This is the most CPU-intensive component due to the predictive pre-warm analytics and geofencing formulas. Scale to at least 3-5 nodes.
- **TB-MQTT-Transport**: Dedicated microservices solely for maintaining persistent MQTT TCP connections.
- **Kafka / RabbitMQ**: Used as the message broker between the MQTT Transport and Rule Engine. Kafka is strongly recommended for its high-throughput partitioning.
- **Cassandra DB**: Migrate from PostgreSQL to Apache Cassandra for handling high-volume time-series telemetry data without bottlenecking.

## 2. High-Volume Device Provisioning

Currently, `coolcycle_provisioning.ino` uses a single `PROVISION_KEY` and `PROVISION_SECRET`.

To scale device provisioning securely:
1. **X.509 Certificates**: Instead of basic access tokens, inject a unique X.509 client certificate into each ESP32 during manufacturing.
2. **Automated Factory Scripts**: Write a Python script that flashes the firmware, requests the ThingsBoard API to generate device credentials, and pushes the keys over serial to the ESP32 NVS partition automatically.
3. **QR Code Claiming**: Use the ThingsBoard Mobile App "Device Claiming" feature. The factory prints a QR code (MAC Address + Secret Key) and sticks it to the device. A clinic worker scans it, and the device is instantly bound to their specific Tenant/Customer hierarchy without admin intervention.

## 3. Node.js Portal Scalability

The `coolcycle_jwt_bridge.js` is a lightweight Express app. To scale it:
- **Serverless**: Deploy it as an AWS Lambda or Vercel Edge Function. It is stateless and relies entirely on ThingsBoard for session state, making it a perfect candidate for infinite horizontal scaling.
- **Redis Caching**: If querying ThingsBoard for user dashboard IDs becomes a bottleneck, implement a Redis cache layer for the `DASHBOARD_IDS` mapping.
