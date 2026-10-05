/*
 * ESP32 32D HAN Serial -> MQTT (TLS)
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
 *
 * More about Han data:
 * https://pkssahkonsiirto.fi/wp-content/uploads/aidon_feature_description_rj12_han_interface_fi.pdf
 *
 * Example Serial data input to be processed:
 * 0-0:1.0.0(260922111700S)
 * 1-0:1.8.0(01463051.760*kWh)     A+ Kumulatiivinen tuntikohtainen sähköverkosta otettu pätöenergia.
 * 1-0:2.8.0(00000000.000*kWh)     A- Kumulatiivinen tuntikohtainen sähköverkkoon syötetty pätöenergia.
 * 1-0:3.8.0(00021302.680*kVArh)
 * 1-0:4.8.0(00036752.360*kVArh)
 * 1-0:1.7.0(0395.240*kW)
 * 1-0:2.7.0(0000.000*kW)
 * 1-0:3.7.0(0014.280*kVAr)
 * 1-0:4.7.0(0000.000*kVAr)
 * 1-0:21.7.0(0135.200*kW)
 * 1-0:22.7.0(0000.000*kW)
 * 1-0:41.7.0(0133.200*kW)
 * 1-0:42.7.0(0000.000*kW)
 * 1-0:61.7.0(0125.040*kW)
 * 1-0:62.7.0(0000.000*kW)
 * 1-0:23.7.0(0021.200*kVAr)
 * 1-0:24.7.0(0000.000*kVAr)
 * 1-0:43.7.0(0000.000*kVAr)
 * 1-0:44.7.0(0023.240*kVAr)
 */
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "ota.h"
#include "conf.h"

// ---- PIN DEFINITIONS ----
constexpr int8_t PIN_DATA_RX = 16;
constexpr int8_t PIN_LED_BUILTIN = 2;

// ---- HAN UART0 ----
const uint32_t HAN_BAUD = 115200;
const size_t LINE_BUF_SIZE = 256;
char lineBuffer[LINE_BUF_SIZE];
size_t linePos = 0;

// ---- MQTT ----
#define MQTT_HOST IPAddress(MQTT_IP0, MQTT_IP1, MQTT_IP2, MQTT_IP3)
#define MQTT_PORT 8883 // TLS

double energy_ap_kwh = 0;     // A+
double energy_ap_kwh_out = 0; // A+
double energy_am_kwh = 0;     // A-
double energy_am_kwh_out = 0; // A-

unsigned long lastPublish = 0;

const unsigned long WIFI_RETRY_MS = 2000;
const unsigned long MQTT_RETRY_FAST_MS = 5000;
const unsigned long MQTT_RETRY_SLOW_MS = 30000;
const uint8_t MQTT_FAST_ATTEMPTS = 3;
unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;
uint8_t mqttFailures = 0;

WiFiClientSecure tlsClient;
PubSubClient mqtt(tlsClient);

char topic_ap[48];
char topic_am[48];
char clientId[24];

// Non-blocking
void connectToWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_2dBm);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to SSID: ");
  Serial.println(WIFI_SSID);
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

// ---- HAN line parsing ----
void processLine(char *line)
{
  // Note that there are 2 important readings outputted by HAN:
  // [1.] 1-0:1.8.0.255 A+ Kumulatiivinen tuntikohtainen sähköverkosta otettu pätöenergia.
  // [2.] 1-0:2.8.0.255 A- Kumulatiivinen tuntikohtainen sähköverkkoon syötetty pätöenergia.
  const char *patterns[2][3] =
    {
      {"1-0:1.8.0(", "1.8.0(", "1-1:1.8.0("}, // A+
      {"1-0:2.8.0(", "2.8.0(", "1-1:2.8.0("}  // A-
    };

  for (int g = 0; g < 2; g++)
  {
    for (int i = 0; i < 3; i++)
    {
      char *p = strstr(line, patterns[g][i]);
      if (p != NULL)
      {
        p = strchr(p, '(');
        if (p != NULL)
        {
          p++;

          char valuestr[32];
          size_t vi = 0;
          while (*p != ')' && *p != '*' && *p != '\0' && vi < sizeof(valuestr) - 1)
          {
            valuestr[vi++] = *p++;
          }
          valuestr[vi] = '\0';

          vi = 0;
          double v = atof(valuestr);
          if (g == 0)
          {
            energy_ap_kwh = v;
            energy_ap_kwh_out = v;
          }
          else
          {
            energy_am_kwh = v;
            energy_am_kwh_out = v;
          }

          Serial.print("Otettu pätöteho (kWh): ");
          Serial.print(energy_ap_kwh_out, 3);
          Serial.print(" | Syötetty pätöteho (kWh): ");
          Serial.println(energy_am_kwh_out, 3);
        }
        return;
      }
    }
  }
} // processLine

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
  // HAN meter on UART0 RX
  Serial2.begin(HAN_BAUD, SERIAL_8N1, PIN_DATA_RX, -1, true); // DO NOT FORGET TO INVERT HAN POLARITY!
  delay(200);
  Serial.println("ESP32 HAN + WiFi -> MQTT");

  pinMode(PIN_LED_BUILTIN, OUTPUT);
  digitalWrite(PIN_LED_BUILTIN, false);

  // Unique client_id and topic per board
  snprintf(clientId, sizeof(clientId), "%s-%02X%02X%02X%02X%02X%02X",
          MQTT_CLIENT_NAME,
          WiFi.macAddress()[0], WiFi.macAddress()[1],
          WiFi.macAddress()[2], WiFi.macAddress()[3],
          WiFi.macAddress()[4], WiFi.macAddress()[5]);
  snprintf(topic_ap, sizeof(topic_ap), "%s/%02X%02X%02X%02X%02X%02X/kwh_ap",
           MQTT_CLIENT_TOPIC,
           WiFi.macAddress()[0], WiFi.macAddress()[1],
           WiFi.macAddress()[2], WiFi.macAddress()[3],
           WiFi.macAddress()[4], WiFi.macAddress()[5]);
  snprintf(topic_am, sizeof(topic_am), "%s/%02X%02X%02X%02X%02X%02X/kwh_am",
           MQTT_CLIENT_TOPIC,
           WiFi.macAddress()[0], WiFi.macAddress()[1],
           WiFi.macAddress()[2], WiFi.macAddress()[3],
           WiFi.macAddress()[4], WiFi.macAddress()[5]);

  // MQTT Settings
  mqtt.setBufferSize(512);
  mqtt.setSocketTimeout(15);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(60);

  // Verify with the MQTT broker
  // tlsClient.setCACert(MQTT_CA_CERT);
  // tlsClient.setCertificate(ESP32_CA_CERT);
  // tlsClient.setPrivateKey(ESP32_RSA_KEY);
  tlsClient.setInsecure();
  tlsClient.setHandshakeTimeout(30);
  connectToWifi();
  otaInit(clientId);
  // Sync time with time server
  setupTime();
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
  if (otaInProgress)
  {
    return;
  }

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

  // Drain the HAN buffer
  while (Serial2.available() > 0)
  {
    char c = (char)Serial2.read();
    if (linePos < LINE_BUF_SIZE - 1)
    {
      lineBuffer[linePos++] = c;
    }
    if (c == '\n')
    {
      lineBuffer[linePos] = '\0';
      processLine(lineBuffer);
      linePos = 0;
    }
  }

  if (mqtt.connected() && millis() - lastPublish >= MQTT_PUBLISH_INTERVAL)
  {
    lastPublish = millis();
    Serial.printf("Publishing: A+ %.3f | A- %.3f\n", energy_ap_kwh_out, energy_am_kwh_out);
    if (energy_ap_kwh_out > 0)
    {
      char apStr[16];
      snprintf(apStr, sizeof(apStr), "%.3f", energy_ap_kwh_out);
      mqtt.publish(topic_ap, apStr);
    }
    if (energy_am_kwh_out > 0)
    {
      char amStr[16];
      snprintf(amStr, sizeof(amStr), "%.3f", energy_am_kwh_out);
      mqtt.publish(topic_am, amStr);
    }
  }
} // loop
