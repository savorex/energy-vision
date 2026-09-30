/**
 * ESP32 Solar Inverter MQTT Simulator
 *
 * Simulates a rooftop PV inverter and publishes telemetry over MQTT over TLS
 * (WiFiClientSecure + PubSubClient, port 8883).
 *
 * Three buttons pick the simulated conditions:
 *   BTN_BAD    -> bad weather / night  (production ~0)
 *   BTN_NORMAL -> normal day           (nominal curve)
 *   BTN_SUNNY  -> full sun             (~135 % of nominal)
 *
 * Production follows the wall clock (time.h, NTP-synced, Europe/Helsinki).
 *
 * Wiring: push buttons between GPIO and GND (INPUT_PULLUP, active-low):
 *   GPIO 32 -> BAD    GPIO 33 -> NORMAL    GPIO 27 -> SUNNY
 *
 * platformio.ini:
 *   [env:esp32dev]
 *   platform = espressif32
 *   board = esp32dev
 *   framework = arduino
 *   monitor_speed = 115200
 *   lib_deps = knolleary/PubSubClient @ ^2.8
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
constexpr uint8_t BTN_BAD = 32;
constexpr uint8_t BTN_NORMAL = 33;
constexpr uint8_t BTN_SUNNY = 27;
constexpr uint8_t LED_MODE = 2; // onboard LED blinks faster in SUNNY

enum class Weather : uint8_t
{
  BAD_NIGHT = 0,
  NORMAL = 1,
  SUNNY = 2
};
Weather weather = Weather::NORMAL;

struct Button
{
  uint8_t pin;
  bool last = HIGH;
  bool stable = HIGH;
  uint32_t lastChange = 0;
};

Button btnBad, btnNormal, btnSunny;

bool update(Button &b) // true once per debounced falling edge (press)
{
  bool now = digitalRead(b.pin);
  bool hit = false;
  if (now != b.last)
  {
    b.last = now;
    b.lastChange = millis();
  }
  if (millis() - b.lastChange > 50) // debounced
  {
    if (now == LOW && b.stable == HIGH)
      hit = true;
    b.stable = now;
  }
  return hit;
}

const char *modeName(Weather w)
{
  switch (w)
  {
  case Weather::BAD_NIGHT:
    return "bad-night";
  case Weather::NORMAL:
    return "normal";
  default:
    return "sunny";
  }
}

void selectMode(Weather w)
{
  if (weather != w)
  {
    weather = w;
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
 * @brief Nominal clear-sky production curve, Gaussian around solar noon.
 * @param hourOfDay fractional hour 0..24
 * @return kW
 */
float solarCurve(float hourOfDay)
{
  const float peakKw = 4.2f;
  float v = peakKw * expf(-0.5f * powf((hourOfDay - 13.0f) / 3.2f, 2.0f));
  return fmaxf(v, 0.0f);
}

float weatherFactor(Weather w)
{
  switch (w)
  {
  case Weather::BAD_NIGHT:
    return 0.08f; // overcast drizzle (or night: curve is 0 anyway)
  case Weather::NORMAL:
    return 0.95f;
  case Weather::SUNNY:
    return 1.35f;
  }
  return 1.0f;
}

// ---------------- State ----------------
float energyTodayKwh = 0.0f;
uint32_t lastPublish = 0;
int lastYday = -1;

const char *TOPIC = TOPIC_PREFIX "/telemetry";
const char *STAT = TOPIC_PREFIX "/status";

void sendTelemetry(float dtSec)
{
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  if (ti.tm_yday != lastYday) // crude midnight reset
  {
    energyTodayKwh = 0.0f;
    lastYday = ti.tm_yday;
  }

  float hourOfDay = ti.tm_hour + ti.tm_min / 60.0f;
  float pvKw = solarCurve(hourOfDay) * weatherFactor(weather);
  pvKw = fmaxf(0.0f, pvKw + (random(-60, 60) / 1000.0f)); // small noise
  float acKw = pvKw * 0.96f;                              // inverter efficiency
  float dcV = pvKw > 0.01f ? 320.0f + 90.0f * (pvKw / 4.2f) : 0.0f;
  float acI = acKw * 1000.0f / 230.0f;
  float freq = 50.0f + (random(-8, 8) / 100.0f);
  float temp = 24.0f + 30.0f * (pvKw / 4.2f);
  energyTodayKwh += acKw * dtSec / 3600.0f;

  char buf[384];
  snprintf(buf, sizeof(buf),
    "{\"device\":\"%s\",\"ts\":%lld,\"mode\":\"%s\","
    "\"pv_power_w\":%.0f,\"ac_power_w\":%.0f,"
    "\"dc_voltage\":%.1f,\"ac_voltage\":230.0,\"ac_current\":%.2f,"
    "\"grid_freq\":%.2f,\"heatsink_temp_c\":%.1f,"
    "\"energy_today_kwh\":%.2f}",
    DEVICE_ID, (long long)now, modeName(weather),
    pvKw * 1000.0f, acKw * 1000.0f, dcV, acI, freq, temp, energyTodayKwh);

  bool ok = client.publish(TOPIC, buf, false);
  Serial.printf("[%02d:%02d:%02d] %s pv=%.2f kW ac=%.2f kW %s\n",
    ti.tm_hour, ti.tm_min, ti.tm_sec, buf + 1, pvKw, acKw,
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
  pinMode(BTN_BAD, INPUT_PULLUP);
  pinMode(BTN_NORMAL, INPUT_PULLUP);
  pinMode(BTN_SUNNY, INPUT_PULLUP);
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

  if (update(btnBad))
  {
    selectMode(Weather::BAD_NIGHT);
  }
  if (update(btnNormal))
  {
    selectMode(Weather::NORMAL);
  }
  if (update(btnSunny))
  {
    selectMode(Weather::SUNNY);
  }
  
  // LED: 1 blink / 2 s bad-night, steady dim normal, fast blink sunny
  {
    uint32_t t = millis() % 2000;
    bool on = (weather == Weather::SUNNY) ? (t < 150) : (weather == Weather::NORMAL) ? true : (t < 400);
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
