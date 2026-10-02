# Savorex ESP32 MQTT Solar Simulator
Connects to an MQTT broker and sends data periodically. Can simulate different weather situations:
1. No sun
2. Normal sun
3. Sunny

## To upload via PlatformIO:
### using IP or by mDNS hostname:
`pio run -t upload --upload-port {IP (192.168.x.x) / mDNS (SIM-WIFIMAC.local)} --upload_flags=--auth={OTA_PASSWORD}`
