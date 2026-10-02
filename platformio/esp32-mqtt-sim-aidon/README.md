# Savorex ESP32 MQTT Aidon Simulator
Connects to an MQTT broker and sends peak power wattage (A+ and A-) data periodically.

## To upload via PlatformIO:
### using IP or by mDNS hostname:
`pio run -t upload --upload-port {IP (192.168.x.x) / mDNS (SIM-WIFIMAC.local)} --upload_flags=--auth={OTA_PASSWORD}`
