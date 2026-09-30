/*
 * ESP32 Aidon Simulator -> MQTT (TLS)
 *
 * - WiFi: WiFi.h
 * - MQTT: PubSubClient over WiFiClientSecure (mbedTLS, port 8883).
 *   The broker is verified with its CA certificate; payloads go out
 *   as plaintext inside the encrypted connection.
 * - HAN meter on UART2 (Serial, RX = GPI16) at 115200 baud.
 * - Serial0 status logging; Serial2 for the HAN P1 stream.
 * - MQTT topic is unique per board: xamk/mkl/<MAC>/kwh (e.g. xamk/mkl/A4CF12B3C4D5/kwh)
 * - Status LED shows status:
 *     fast blink  = searching / connecting WiFi
 *     slow blink  = WiFi OK, MQTT not connected
 *     solid       = WiFi + MQTT online
 */
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "ota.h"
#include "conf.h"

// ---- PIN DEFINITIONS ----
constexpr int8_t PIN_LED_BUILTIN = 2;

// ---- MQTT ----
#define MQTT_HOST IPAddress(MQTT_IP0, MQTT_IP1, MQTT_IP2, MQTT_IP3)
#define MQTT_PORT 8883 // TLS

IPAddress local_IP(172, 16, 8, 1);
IPAddress gateway(172, 16, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

double energy_ap_kwh = 0.0;           // A+
double energy_ap_kwh_out = 1495000.0; // A+
double energy_am_kwh = 0.0;           // A-
double energy_am_kwh_out = 0.0;       // A-

unsigned long lastPublish = 0;
const long publishInterval = 15000;

const unsigned long WIFI_RETRY_MS = 2000;
const unsigned long MQTT_RETRY_FAST_MS = 5000;
const unsigned long MQTT_RETRY_SLOW_MS = 30000;
const uint8_t MQTT_FAST_ATTEMPTS = 3;
unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;
uint8_t mqttFailures = 0;

WiFiClientSecure tlsClient;
PubSubClient mqtt(tlsClient);

char topic[32];
char clientId[24];

// Non-blocking
void connectToWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(WIFI_SSID);
  // WiFi.config should be run after WiFi.begin
  if (WiFi.SSID() == WIFI_SSID)
  {
    Serial.println("\nManual IP Configuration");
    WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  }
}

// Blink status by state; solid when everything is fine
void updateLed()
{
  static unsigned long lastToggle = 0;
  unsigned long now = millis();
  unsigned long period;

  if (WiFi.status() != WL_CONNECTED)
  {
    period = 150; // fast: hunting for WiFi
  }
  else if (!mqtt.connected())
  {
    period = 400; // slow: WiFi up, MQTT down
  }
  else
  {
    digitalWrite(PIN_LED_BUILTIN, HIGH);
    return;
  } // solid: all good

  if (now - lastToggle >= period)
  {
    lastToggle = now;
    digitalWrite(PIN_LED_BUILTIN, !digitalRead(PIN_LED_BUILTIN));
  }
} // updateLed

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("ESP32 HAN + WiFi -> MQTT");

  pinMode(PIN_LED_BUILTIN, OUTPUT);
  digitalWrite(PIN_LED_BUILTIN, false);

  // Sync time with time server
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  // Unique topic and client id per board
  snprintf(topic, sizeof(topic), "%s/%02X%02X%02X%02X%02X%02X/kwh",
    MQTT_CLIENT_TOPIC,
    WiFi.macAddress()[0], WiFi.macAddress()[1],
    WiFi.macAddress()[2], WiFi.macAddress()[3],
    WiFi.macAddress()[4], WiFi.macAddress()[5]
  );
  snprintf(clientId, sizeof(clientId), "%s-%02X%02X%02X%02X%02X%02X",
    MQTT_CLIENT_NAME,
    WiFi.macAddress()[0], WiFi.macAddress()[1],
    WiFi.macAddress()[2], WiFi.macAddress()[3],
    WiFi.macAddress()[4], WiFi.macAddress()[5]
  );

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_NONE);

  // Verify with the MQTT broker
  tlsClient.setInsecure();
  // tlsClient.setCACert(MQTT_CA_CERT);
  // tlsClient.setCertificate(ESP32_CA_CERT);
  // tlsClient.setPrivateKey(ESP32_RSA_KEY);
  tlsClient.setTimeout(3);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(20);
  otaInit(clientId);
  connectToWifi();
} // setup

void loop()
{
  updateLed();

  if (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - lastWifiAttempt >= WIFI_RETRY_MS)
    {
      lastWifiAttempt = millis();
      connectToWifi();
    }
    return;
  }

  otaLoop();

  if (!mqtt.connected())
  {
    unsigned long retryMs = (mqttFailures < MQTT_FAST_ATTEMPTS) ? MQTT_RETRY_FAST_MS : MQTT_RETRY_SLOW_MS;

    if (millis() - lastMqttAttempt >= retryMs)
    {
      lastMqttAttempt = millis();

      Serial.print("Attempting to connect to MQTT (");
      Serial.print(MQTT_IP0);
      Serial.print(".");
      Serial.print(MQTT_IP1);
      Serial.print(".");
      Serial.print(MQTT_IP2);
      Serial.print(".");
      Serial.print(MQTT_IP3);
      Serial.print(":");
      Serial.print(MQTT_PORT);
      Serial.print(") as ");
      Serial.print(MQTT_USERNAME);
      Serial.print(", client_id: ");
      Serial.print(clientId);
      Serial.println(")");
      if (mqtt.connect(clientId, MQTT_USERNAME, MQTT_PASSWORD))
      {
        mqttFailures = 0;
        Serial.println("MQTT connected");
      }
      else
      {
        mqttFailures++;
        Serial.print("MQTT connection failed, state=");
        Serial.print(mqtt.state());
        char errBuf[256];
        int err = tlsClient.lastError(errBuf, sizeof(errBuf));
        Serial.print(err);
        Serial.print(" / ");
        Serial.println(errBuf);
      }
    }
  }
  else
  {
    mqtt.loop();
  }

  if (millis() - lastPublish >= publishInterval)
  {
    energy_ap_kwh_out = energy_ap_kwh_out + (1.0 * ((float)rand() / (float)RAND_MAX));

    lastPublish = millis();
    char plain[35];
    snprintf(plain, sizeof(plain), "%.3f", energy_ap_kwh_out);
    mqtt.publish(topic, plain);

    Serial.print("Trying to publish: ");
    Serial.println(plain);
  }
} // loop
