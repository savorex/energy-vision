# Savorex Energy Vision

Reads an Aidon HAN electricity meter, publishes consumption over MQTT (TLS), and visualises it in a web app. Additional solar or wind production can be fed in with the included simulators.

```
Aidon HAN meter - P1/UART──▶ ESP32 Sender ──MQTT/TLS :8883──▶ MQTT ◀── Savorex Energy (web)
                                  ▲                            ▲
                       ESP32MQTT Simulator Aidon        ESP32MQTT Simulator Solar
                          (no meter needed)            (simulated production data)
```

## Integrations

| Use-case | Description | Link |
|---|---|---|
| **XAMK Energy** | Campus consumption data sankey-diagram | https://hanmeter.duckdns.org/xamk-energy/

## Components

| Component | What it is | Language / platform |
|---|---|---|
| **ESP32 MQTT Sender Aidon** | Real firmware: reads the HAN meter over UART, publishes kWh | C++ (PlatformIO, ESP32) |
| **ESP32MQTT Simulator Aidon** | Pretends to be the sender: publishes realistic consumption without a meter | C++ (PlatformIO, ESP32) |
| **ESP32MQTT Simulator Solar** | Pretends to be an inverter: publishes solar production | C++ (PlatformIO, ESP32) |
| **ESP32MQTT Simulator Wind** | Pretends to be an inverter: publishes solar production | C++ (PlatformIO, ESP32) |
| **Savorex Energy (web)** | Dashboard that consumes the MQTT data | Powered via Node.js |

## Repository layout

```
.
├── esp32-mqtt-dev-aidon/     ESP32 MQTT Sender Aidon (main.cpp)
├── esp32-mqtt-sim-aidon/     ESP32 Aidon metering device simulator (WIP)
├── esp32-mqtt-sim-solar/     ESP32 PV device simulator (WIP)
├── energy-app/               Sankey Energy App
└── nginx/                    Reverse proxy configs (OPTIONAL)
```

## Components in detail

### 1. ESP32 MQTT Sender Aidon

Firmware for an ESP32 wired to the Aidon meter's HAN/P1 port.

- **HAN reading:** the meter streams data over UART at 115200 baud with **inverted polarity** — remember the invert flag on `Serial2.begin()`. The parser looks for the OBIS code `1-0:1.8.0(` (active energy imported, kWh) and tolerates the shorter `1.8.0(` variants.
- **Publishing:** the parsed value is published every 15 s.
- **MQTT/TLS:** connects to Mosquitto on port **8883** via `WiFiClientSecure`, verifying the broker with a CA certificate (`setCACert` — do *not* use `setInsecure()` in production).
- **Identity:** topic and client ID derive from the WiFi MAC, so every board is unique without configuration:
  - topic: `han/<MAC>/kwh`
  - client ID: `han-<MAC>`
- **Status LED:** fast blink = looking for WiFi, slow blink = WiFi up / MQTT down, solid = fully online.
- **Resilience:** non-blocking reconnect loop (WiFi retry 2 s, MQTT retry 5 s); `mqtt.loop()` runs every iteration so the keepalive is never starved.

> **Note:** during development the exact 60 s disconnects were traced to keepalive handling and idle timeouts — see the troubleshooting section below.

### 2. ESP32MQTT Simulator Aidon

Publishes the same topic shape as the real sender, so you can develop the app side without hardware.

### 3. ESP32MQTT Simulator Solar

Publishes solar production data alongside consumption.

### 4. Energy App (dashboard)

Subscribes to the MQTT topics and visualises live consumption and production.

## Topics

| Topic | Direction | Payload | Interval |
|---|---|---|---|
| `xamkenergy/<MAC>/ap_kwh` | device => broker | (cumulative A+ energy) kWh | 15 s |
| `xamkenergy/<MAC>/am_kwh` | device => broker | (cumulative A- energy) kWh | 15 s |
| `solar` | simulator => broker | 0.00 - 20.00 kWh | 15s |
| `wind` | simulator => broker | 0.00 - 15.00 kWh | 15s |

## Broker setup (mosquitto-tls/)

1. Generate certs: `./gen-certs.sh` (server cert signed by your own CA; `letsencrypt-setup.md` covers the Let's Encrypt alternative with a deploy hook).
2. Create credentials: `mosquitto_passwd -c ./mosquitto/passwd <user>`.
3. Start: `docker compose up -d`.

Listeners:

- **8883** — MQTT over TLS (`protocol mqtt`, server + CA certs mounted).
- **1884** — plaintext MQTT for quick local debugging (still password-protected).

## TLS Handshake:
Quick end-to-end checks:

```bash
# TLS handshake + cert verification
openssl s_client -connect <broker-host>:8883 -CAfile ca.crt -verify_return_error

# Publish a test value
mosquitto_pub -h <broker-host> -p 8883 --cafile ca.crt -u <user> -P <pass> -t aidon/test/kwh -m 42.0
```

### Generating a PEM key for TLS handshakes (Broker and ESP32):
```
openssl req -new -x509 -days 365 -extensions v3_ca -keyout ca.key -out ca.crt -passout pass:1234 -subj '/CN=myserver.dynamic-dns.net'

openssl genrsa -out mosquitto.key 2048
openssl req -out mosquitto.csr -key mosquitto.key -new -subj '/CN=localhost'
openssl x509 -req -in mosquitto.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out mosquitto.crt -days 365 -passin pass:1234

openssl genrsa -out esp.key 2048
openssl req -out esp.csr -key esp.key -new -subj '/CN=localhost'
openssl x509 -req -in esp.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out esp.crt -days 365 -passin pass:1234
```

## PlatformIO upload:
`platformio run -e esp32dev -t upload`

After first flash:
`platformio run -e esp32ota -t upload`

## Troubleshooting notes (learned the hard way)

- **`-9984 X509_CERT_VERIFY_FAILED` on the ESP32** → wrong CA, mangled PEM, or the device clock is still at 1970. Sync time (`configTime` + NTP) *before* connecting.
- **Connection by IP fails cert validation** — certificates match hostnames, not IPs. Use the broker's DNS name.
- **Timeout disconnects:** the client must send traffic within the keepalive window. `mqtt.loop()` must run every loop iteration — a blocking HAN read or `delay()` starves it and the broker drops the connection.
- **Docker containers aren't pingable from the LAN** — test the *host's* published port instead: `nc -zv <broker-host> 8883`.
