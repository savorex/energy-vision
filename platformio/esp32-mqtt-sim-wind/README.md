# Savorex ESP32 MQTT Wind Simulator
Connects to an MQTT broker and sends data periodically. Can simulate different weather situations:
1. No wind
2. Normal wind
3. Windy

## To upload via PlatformIO:
### using IP:
`pio run -t upload --upload-port 192.168.x.x`
### or by mDNS hostname:
`pio run -t upload --upload-port solar-sim-01.local`
