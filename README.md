# Home Energy Monitoring — ESP32 + MQTT + Web

Reads an Aidon HAN electricity meter, publishes consumption over MQTT (TLS), and visualises it in a web app. Solar production can be fed in with the included simulators.

```
Aidon HAN meter ──P1/UART──▶ ESP32 Sender ──MQTT/TLS :8883──▶ Mosquitto ◀── Energy App (web)
                                  ▲                            ▲
                       ESP32MQTT Simulator Aidon        ESP32MQTT Simulator Solar
                       (no meter needed)                (fake production data)
```

## Components

| Component | What it is | Language / platform |
|---|---|---|
| **ESP32 MQTT Sender Aidon** | Real firmware: reads the HAN meter over UART, publishes kWh | C++ (Arduino/PlatformIO, ESP32) |
| **ESP32MQTT Simulator Aidon** | Pretends to be the sender: publishes realistic consumption without a meter | <!-- TODO: Python / Node / Arduino? --> |
| **ESP32MQTT Simulator Solar** | Pretends to be an inverter: publishes solar production | <!-- TODO --> |
| **Energy App (web)** | Dashboard that consumes the MQTT data | <!-- TODO: framework, e.g. Node + socket.io --> |
| **mosquitto-tls/** | Broker deployment: Mosquitto with TLS on 8883 (fallback plaintext 1884) | Docker Compose |

## Repo layout

```
.
├── firmware/            ESP32 MQTT Sender Aidon (main.cpp)
├── simulators/          TODO: where do the two simulators live?
├── energy-app/          TODO: Energy App (web)
├── mosquitto-tls/       Broker: docker-compose.yml, mosquitto.conf, gen-certs.sh
└── nginx/               Optional reverse proxy configs
```

*(Adjust the tree above to match the real repo.)*

## Components in detail

### 1. ESP32 MQTT Sender Aidon

Firmware for an ESP32 wired to the Aidon meter's HAN/P1 port.

- **HAN reading:** the meter streams data over UART at 115200 baud with **inverted polarity** — remember the invert flag on `Serial2.begin()`. The parser looks for the OBIS code `1-0:1.8.0(` (active energy imported, kWh) and tolerates the shorter `1.8.0(` variants.
- **Publishing:** the parsed value is published every 15 s.
- **MQTT/TLS:** connects to Mosquitto on port **8883** via `WiFiClientSecure`, verifying the broker with a CA certificate (`setCACert` — do *not* use `setInsecure()` in production).
- **Identity:** topic and client ID derive from the WiFi MAC, so every board is unique without configuration:
  - topic: `aidon/<MAC>/kwh`
  - client ID: `aidon-<MAC>`
- **Status LED:** fast blink = looking for WiFi, slow blink = WiFi up / MQTT down, solid = fully online.
- **Resilience:** non-blocking reconnect loop (WiFi retry 2 s, MQTT retry 5 s); `mqtt.loop()` runs every iteration so the keepalive is never starved.

> **Note:** during development the exact 60 s disconnects were traced to keepalive handling and idle timeouts — see the troubleshooting section below.

### 2. ESP32MQTT Simulator Aidon

Publishes the same topic shape as the real sender, so you can develop the app side without hardware. <!-- TODO: exact usage: `python simulators/aidon_sim.py --broker host --interval 15`? -->

### 3. ESP32MQTT Simulator Solar

Publishes solar production data alongside consumption. <!-- TODO: topic name, payload format, day-curve behaviour -->

### 4. Energy App (web)

Subscribes to the MQTT topics and visualises live consumption and production. <!-- TODO: how it connects to the broker (MQTT-over-websockets? backend relay?), how to run it, screenshot -->

## Topics

| Topic | Direction | Payload | Interval |
|---|---|---|---|
| `aidon/<MAC>/kwh` | sender → broker | cumulative energy in kWh, e.g. `12345.678` | 15 s |
| *(solar topic — TODO)* | simulator → broker | <!-- TODO: W? kWh? --> | <!-- TODO --> |

## Broker setup (mosquitto-tls/)

1. Generate certs: `./gen-certs.sh` (server cert signed by your own CA; `letsencrypt-setup.md` covers the Let's Encrypt alternative with a deploy hook).
2. Create credentials: `mosquitto_passwd -c ./mosquitto/passwd <user>`.
3. Start: `docker compose up -d`.

Listeners:

- **8883** — MQTT over TLS (`protocol mqtt`, server + CA certs mounted).
- **1884** — plaintext MQTT for quick local debugging (still password-protected).

Quick end-to-end checks:

```bash
# TLS handshake + cert verification
openssl s_client -connect <broker-host>:8883 -CAfile ca.crt -verify_return_error

# Publish a test value
mosquitto_pub -h <broker-host> -p 8883 --cafile ca.crt -u <user> -P <pass> -t aidon/test/kwh -m 42.0
```

## Troubleshooting notes (learned the hard way)

- **`-9984 X509_CERT_VERIFY_FAILED` on the ESP32** → wrong CA, mangled PEM, or the device clock is still at 1970. Sync time (`configTime` + NTP) *before* connecting.
- **Connection by IP fails cert validation** — certificates match hostnames, not IPs. Use the broker's DNS name.
- **Timeout disconnects:** the client must send traffic within the keepalive window. `mqtt.loop()` must run every loop iteration — a blocking HAN read or `delay()` starves it and the broker drops the connection.
- **Docker containers aren't pingable from the LAN** — test the *host's* published port instead: `nc -zv <broker-host> 8883`.
