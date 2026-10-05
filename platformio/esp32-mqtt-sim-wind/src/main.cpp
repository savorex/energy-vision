/**
 * ESP32 Wind Generator MQTT Simulator
 *
 * Simulates a small wind turbine and publishes telemetry over MQTT over TLS
 * (WiFiClientSecure + PubSubClient, port 8883).
 *
 * Three buttons pick the simulated conditions:
 *   PIN_BTN_CALM    -> no wind      (below cut-in speed, ~0 W)
 *   PIN_BTN_NORMAL  -> normal wind  (~7.5 m/s base, modest gusts)
 *   PIN_BTN_WINDY   -> windy        (~14 m/s base, hard gusts, rated output)
 *
 * Wind speed follows a gentle diurnal cycle from the wall clock (time.h,
 * NTP-synced, Europe/Helsinki) plus random gusting.
 *
 * Wiring: push buttons between GPIO and GND (INPUT_PULLUP, active-low):
 *    GPIO 32 -> CALM
 *    GPIO 33 -> NORMAL
 *    GPIO 27 -> WINDY
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <math.h>
#include "ota.h"
#include "conf.h"

uint8_t wifi_status = 0;

float energyKwh = 0.0f;
float energyTotalkWh = 0.0f;
uint32_t lastPublish = 0;

char DEVICE_MAC[18];
char TOPIC[48];
char STAT[48];

WiFiClientSecure tlsClient;
PubSubClient client(tlsClient);

constexpr uint8_t PIN_MODE_LED = 32;
constexpr uint8_t PIN_MODE_LED2 = 16;

constexpr uint8_t PIN_BTN_CALM = 33;
constexpr uint8_t PIN_BTN_NORMAL = 26;
constexpr uint8_t PIN_BTN_WINDY = 27;

enum class Wind : uint8_t
{
  CALM = 0,
  NORMAL = 1,
  WINDY = 2
};
Wind wind = Wind::NORMAL;

const char *modeName(Wind w)
{
  switch (w)
  {
  case Wind::CALM:
    return "calm";
  case Wind::NORMAL:
    return "normal";
  default:
    return "windy";
  }
}

void selectMode(Wind w)
{
  if (wind != w)
  {
    wind = w;
    Serial.printf("[mode] -> %s\n", modeName(w));
  }
}

bool setupTime()
{
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1); // Europe/Helsinki
  tzset();

  Serial.print("[time] waiting for NTP");
  uint32_t start = millis();
  while (time(nullptr) < 1600000000 && millis() - start < 12000)
  {
    delay(500);
    Serial.print(".");
  }

  bool ok = time(nullptr) >= 1600000000;
  Serial.println(ok ? " ok" : " FAILED");
  return ok;
}

/**
 * @brief Wind speed right now: mode base + diurnal cycle + gusts. The diurnal cycle comes from time.h (peak wind in the afternoon).
 * @return m/s
 */
float windSpeed(float hourOfDay)
{
  float base = 0.0f;
  float gustAmp = 0.0f;
  switch (wind)
  {
  case Wind::CALM:
    base = 1.4f;
    gustAmp = 0.6f;
    break;
  case Wind::NORMAL:
    base = 7.5f;
    gustAmp = 1.6f;
    break;
  case Wind::WINDY:
    base = 14.0f;
    gustAmp = 3.5f;
    break;
  }
  // Afternoon wind maximum, small amplitude
  float diurnal = 1.8f * sinf((hourOfDay - 9.0f) / 24.0f * 2.0f * (float)M_PI);
  float gust = random(-100, 100) / 100.0f * gustAmp;
  return fmaxf(0.0f, base + diurnal + gust);
}

/**
 * @brief Simplified turbine power curve. cut-in 3 m/s, rated 3.0 kW at 12 m/s, cut-out 25 m/s, cubic ramp.
 * @return kW
 */
float turbinePower(float v)
{
  const float cutIn = 3.0f;
  const float ratedV = 12.0f;
  const float cutOut = 25.0f;
  const float ratedP = 3.0f; // kW

  if (v < cutIn || v > cutOut)
    return 0.0f; // cut-out also acts as "over-speed fault"
  if (v >= ratedV)
    return ratedP;
  float f = (v - cutIn) / (ratedV - cutIn);
  return ratedP * f * f * f;
}

void sendTelemetry(float dtSec)
{
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  float hourOfDay = ti.tm_hour + ti.tm_min / 60.0f;

  float v = windSpeed(hourOfDay);
  float kw = turbinePower(v);
  kw = fmaxf(0.0f, kw + (random(-20, 20) / 1000.0f));
  float rpm = fminf(v * 26.0f, 40.0f); // roughly proportional to wind
  float temp = 18.0f + 12.0f * (kw / 3.0f);
  bool overSpeed = v > 25.0f;
  energyKwh += kw * dtSec / 3600.0f;
  energyTotalkWh += kw * dtSec;

  char buf[384];
  snprintf(buf, sizeof(buf),
    "{\"device\":\"%s\",\"ts\":%lld,\"mode\":\"%s\","
    "\"wind_speed_ms\":%.1f,\"rpm\":%.0f,\"power_w\":%.0f,"
    "\"ac_voltage\":230.0,\"ac_current\":%.2f,"
    "\"grid_freq\":%.2f,\"gearbox_temp_c\":%.1f,"
    "\"brake\":%s,\"energy_total_kwh\":%.2f}",
    DEVICE_ID, (long long)now, modeName(wind),
    v, rpm, kw * 1000.0f, kw * 1000.0f / 230.0f,
    50.0f + (random(-8, 8) / 100.0f), temp,
    overSpeed ? "true" : "false", energyKwh);

  char topic[48];
  snprintf(topic, sizeof(topic), "%s/%s/kwh_acc", TOPIC_PREFIX, DEVICE_MAC);
  char msg[24];
  snprintf(msg, sizeof(msg), "%.3f", energyTotalkWh);
  bool ok = client.publish(topic, msg, false);
  Serial.printf("[%02d:%02d:%02d] %s pv=%.2f kW ac=%.2f kW %s\n",
    ti.tm_hour, ti.tm_min, ti.tm_sec, buf + 1, energyTotalkWh, kw,
    ok ? "published" : "PUBLISH FAILED");
}

void connect()
{
  if (wifi_status == 2)
  {
    while (!client.connected())
    {
      Serial.printf("[mqtt] connecting to %s:%d (TLS)...", MQTT_SERVER, MQTT_PORT);
      // LWT: if the device drops, the broker publishes offline (retained)
      bool ok = client.connect(DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD, STAT, 0, true, "offline");
      Serial.println(ok ? " ok" : " failed, retrying in 3 s");
      if (!ok)
      {
        delay(3000);
      }
    }
  }
  otaInit("SolarSim-A53455G");
  if (wifi_status == 2)
  {
    client.publish(STAT, "online", true); // retained
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_BTN_CALM, INPUT);
  pinMode(PIN_BTN_NORMAL, INPUT);
  pinMode(PIN_BTN_WINDY, INPUT);
  pinMode(PIN_MODE_LED, OUTPUT);
  pinMode(PIN_MODE_LED2, OUTPUT);

  uint64_t m = ESP.getEfuseMac(); // ESP32; use WiFi.macAddress() after WiFi.begin()
  snprintf(DEVICE_MAC, sizeof(DEVICE_MAC), "%02X%02X%02X%02X%02X%02X",
    (uint8_t)(m >> 40), (uint8_t)(m >> 32), (uint8_t)(m >> 24),
    (uint8_t)(m >> 16), (uint8_t)(m >> 8), (uint8_t)m);

  snprintf(TOPIC, sizeof(TOPIC), "%s/%s/telemetry", TOPIC_PREFIX, DEVICE_MAC);
  snprintf(STAT, sizeof(STAT), "%s/%s/status", TOPIC_PREFIX, DEVICE_MAC);

  randomSeed(esp_random());

  Serial.printf("[wifi] connecting to %s", SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.printf(" ok, IP %s\n", WiFi.localIP().toString().c_str());
  wifi_status = 1;

  if (setupTime())
  {
    wifi_status = 2;
    //   tlsClient.setCACert(ROOT_CA);
    tlsClient.setInsecure();
    tlsClient.setHandshakeTimeout(30);
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setBufferSize(512);
    client.setKeepAlive(30);
    client.setSocketTimeout(10);
  }

  connect();
  lastPublish = millis();
}

void loop()
{
  if (wifi_status == 0)
  {
    connect();
  }
  else if (wifi_status == 2)
  {
    client.loop();
  }
  otaLoop();

   // read the state of the pushbutton value:
  uint8_t atate = digitalRead(PIN_BTN_WINDY);

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (atate == HIGH) {
    // turn LED on:
    digitalWrite(PIN_MODE_LED2, HIGH);
  } else {
    // turn LED off:
    digitalWrite(PIN_MODE_LED2, LOW);
  }

  // LED: 1 blink / 2 s calm, steady dim normal, fast blink windy
  uint32_t t = millis() % 2000;
  bool on = (wind == Wind::WINDY)
    ? (t < 150)
    : (wind == Wind::NORMAL)
      ? true
      : (t < 400);
  digitalWrite(PIN_MODE_LED, on ? HIGH : LOW);

  uint32_t now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS)
  {
    float dtSec = (now - lastPublish) / 1000.0f;
    lastPublish = now;
    if (wifi_status == 2)
    {
      sendTelemetry(dtSec);
    }
  }

  delay(10);
}
