/**
 * ESP32 Electrical Load Simulator
 */
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "conf.h"

WiFiClient wifi_client;
PubSubClient client(wifi_client);

// Production
float solar_curve(int hour)
{
  float peak = 4.0;
  float value = peak * exp(-0.5 * pow((hour - 12) / 3.5, 2));
  return max(value, 0.0f);
}

// Consumption
float building_load(int hour)
{
  if (hour < 6)
    return 1.2;
  if (hour < 9)
    return 3.5;
  if (hour < 17)
    return 6.0;
  if (hour < 20)
    return 4.0;
  return 1.5;
}

void send_telemetry(void)
{
  time_t now;
  time(&now);
  struct tm *timeinfo = localtime(&now);
  int hour = timeinfo->tm_hour;
  float solar = solar_curve(hour);
  float load = building_load(hour);
  float hvac = load * 0.45;
  float lighting = load * 0.25;
  float ev = (hour >= 18 && hour <= 22) ? 2.5 : 0.5;
  float grid = max(load + ev - solar, 0.0f);
  JsonDocument doc;
  doc["solar"] = solar;
  doc["grid"] = grid;
  doc["hvac"] = hvac;
  doc["lighting"] = lighting;
  doc["ev"] = ev;
  char buffer[256];
  serializeJson(doc, buffer);
  client.publish("v1/devices/me/telemetry", buffer);
  Serial.println(buffer);
}

void setup(void)
{
  Serial.begin(115200);
  WiFi.begin(SSID, PASSWD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
  client.setServer(SERVER, 1883);
}

void loop(void)
{
  if (!client.connected())
  {
    client.connect("ESP32", TOKEN, NULL);
  }
  client.loop();
  send_telemetry();
  delay(5000);
}
