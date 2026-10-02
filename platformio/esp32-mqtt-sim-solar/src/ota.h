/**
 * ota.h — OTA firmware updates via PlatformIO (ArduinoOTA).
 *
 * The device runs the ArduinoOTA listener (port 3232, UDP + TCP) on your LAN.
 * From PlatformIO, flash over WiFi while on the same router:
 *
 *     pio run -t upload --upload-port 192.168.x.x
 *     (or) pio run -t upload --upload-port solar-sim-01.local
 *
 * With OTA_PASS set, PlatformIO sends it via `upload_flags = --auth=...`
 * (see platformio.ini). Upload a fresh `pio run` build of this same project.
 */
#pragma once

#include <ArduinoOTA.h>
#include "conf.h"

uint8_t otaInProgress = 0;

inline void otaInit(const char *device_id)
{
  ArduinoOTA.setHostname(device_id);
  if (OTA_PASSWORD[0] != '\0')
  {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }
  ArduinoOTA.setPort(OTA_PORT);

  ArduinoOTA.onStart([]()
  {
    otaInProgress = 1;
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("[ota] update started (" + type + "), pausing MQTT");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    Serial.printf("[ota] %u %%\r", progress / (total / 100));
    if (progress == total - 1)
    {
      Serial.println();
    }
  });

  ArduinoOTA.onEnd([]()
  {
    otaInProgress = 0;
    Serial.println("\n[ota] done, rebooting");
  });

  ArduinoOTA.onError([](ota_error_t err)
  {
    otaInProgress = 0;
    Serial.printf("[ota] ERROR[%u]: ", err);
    switch (err) {
      case OTA_AUTH_ERROR:
        Serial.println("auth failed");
        break;
      case OTA_BEGIN_ERROR:
        Serial.println("begin failed");
        break;
      case OTA_CONNECT_ERROR:
        Serial.println("connect failed");
        break;
      case OTA_RECEIVE_ERROR:
        Serial.println("receive failed");
        break;
      case OTA_END_ERROR:
        Serial.println("end failed");
        break;
    }
  });

  ArduinoOTA.begin();
  Serial.printf("[ota] ready: %s:3232 (upload with: pio run -t upload --upload-port %s)\n", device_id, WiFi.localIP().toString().c_str());
}

inline void otaLoop()
{
  ArduinoOTA.handle();
}
