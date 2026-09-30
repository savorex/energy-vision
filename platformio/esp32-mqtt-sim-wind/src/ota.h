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

inline bool otaInProgress = false;

inline void otaInit()
{
  ArduinoOTA.setHostname(DEVICE_ID);
  if (OTA_PASS[0] != '\0')
  {
    ArduinoOTA.setPassword(OTA_PASS);
  }

  ArduinoOTA.onStart([]()
  {
    otaInProgress = true;
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
    Serial.println("\n[ota] done, rebooting");
  });

  ArduinoOTA.onError([](ota_error_t err)
  {
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
    otaInProgress = false;
  });

  ArduinoOTA.begin();
  Serial.printf("[ota] ready: %s:3232 (upload with: pio run -t upload --upload-port %s)\n", DEVICE_ID, WiFi.localIP().toString().c_str());
}

inline void otaLoop()
{
  ArduinoOTA.handle();
}
