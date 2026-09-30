/**
 * ESP32 Wind Generator MQTT Simulator
 *
 * Simulates a small wind turbine and publishes telemetry over MQTT over TLS
 * (WiFiClientSecure + PubSubClient, port 8883).
 *
 * Three buttons pick the simulated conditions:
 *   BTN_CALM    -> no wind      (below cut-in speed, ~0 W)
 *   BTN_NORMAL  -> normal wind  (~7.5 m/s base, modest gusts)
 *   BTN_WINDY   -> windy        (~14 m/s base, hard gusts, rated output)
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

WiFiClientSecure tlsClient;
PubSubClient client(tlsClient);

// ---------------- Buttons (active-low) ----------------
constexpr uint8_t BTN_CALM = 32;
constexpr uint8_t BTN_NORMAL = 33;
constexpr uint8_t BTN_WINDY = 27;
constexpr uint8_t LED_MODE = 2; // onboard LED blinks faster when WINDY

enum class Wind : uint8_t
{
  CALM = 0,
  NORMAL = 1,
  WINDY = 2
};
Wind wind = Wind::NORMAL;

struct Button
{
  uint8_t pin;
  bool last = HIGH;
  bool stable = HIGH;
  uint32_t lastChange = 0;
};

Button btnCalm, btnNormal, btnWindy;

bool update(Button &b) // true once per debounced falling edge (press)
{
  bool now = digitalRead(b.pin);
  bool hit = false;
  if (now != b.last)
  {
    b.last = now;
    b.lastChange = millis();
  }
  if (millis() - b.lastChange > 50)
  { // debounced
    if (now == LOW && b.stable == HIGH)
      hit = true;
    b.stable = now;
  }
  return hit;
}

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

// ---------------- Time (NTP + time.h) ----------------
void setupTime()
{
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1); // Europe/Helsinki
  tzset();

  Serial.print("[time] waiting for NTP");
  uint32_t start = millis();
  while (time(nullptr) < 1600000000 && millis() - start < 30000)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" done");
}

// ---------------- Simulation model ----------------
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

// ---------------- State ----------------
float energyKwh = 0.0f;
uint32_t lastPublish = 0;

const char *TOPIC = TOPIC_PREFIX "/telemetry";
const char *STAT = TOPIC_PREFIX "/status";

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

  bool ok = client.publish(TOPIC, buf, false);
  Serial.printf("[%02d:%02d:%02d] %s v=%.1f m/s P=%.0f W %s\n",
                ti.tm_hour, ti.tm_min, ti.tm_sec, buf + 1, v, kw * 1000.0f,
                ok ? "published" : "PUBLISH FAILED");
}

void connect()
{
  while (!client.connected())
  {
    Serial.printf("[mqtt] connecting to %s:%d (TLS)...", SERVER, MQTT_PORT);
    // LWT: if the device drops, the broker publishes offline (retained)
    bool ok = client.connect(DEVICE_ID, TOKEN, NULL, STAT, 0, true, "offline");
    Serial.println(ok ? " ok" : " failed, retrying in 3 s");
    if (!ok)
      delay(3000);
  }
  otaInit();
  client.publish(STAT, "online", true); // retained
}

// ---------------- Arduino ----------------
void setup()
{
  Serial.begin(115200);
  pinMode(BTN_CALM, INPUT_PULLUP);
  pinMode(BTN_NORMAL, INPUT_PULLUP);
  pinMode(BTN_WINDY, INPUT_PULLUP);
  pinMode(LED_MODE, OUTPUT);

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

  setupTime();

  // For a real broker pin the CA instead of setInsecure():
  //   tlsClient.setCACert(ROOT_CA);
  tlsClient.setInsecure(); // demo only: skips certificate validation
  tlsClient.setHandshakeTimeout(30);

  client.setServer(SERVER, MQTT_PORT);
  client.setBufferSize(512); // JSON won't fit in the 256-byte default
  client.setKeepAlive(30);
  client.setSocketTimeout(10);

  connect();
  lastPublish = millis();
}

void loop()
{
  if (!client.connected())
  {
    connect();
  }
  otaLoop();
  client.loop();

  if (update(btnCalm))
    selectMode(Wind::CALM);
  if (update(btnNormal))
    selectMode(Wind::NORMAL);
  if (update(btnWindy))
    selectMode(Wind::WINDY);

  // LED: 1 blink / 2 s calm, steady dim normal, fast blink windy
  {
    uint32_t t = millis() % 2000;
    bool on = (wind == Wind::WINDY)    ? (t < 150)
              : (wind == Wind::NORMAL) ? true
                                       : (t < 400);
    digitalWrite(LED_MODE, on ? HIGH : LOW);
  }

  uint32_t now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS)
  {
    float dtSec = (now - lastPublish) / 1000.0f;
    lastPublish = now;
    sendTelemetry(dtSec);
  }

  delay(10);
}
