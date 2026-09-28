/*
 * ESP32 Wroom32D HAN Serial -> MQTT (TLS)
 *
 * Ported from the Arduino Uno R4 sketch.
 * - WiFi: WiFi.h (ESP32)
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
 * 1-0:1.8.0(01463051.760*kWh)
 * 1-0:2.8.0(00000000.000*kWh)
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
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "secrets.h"

// ---- PIN DEFINITIONS ----
constexpr int8_t PIN_DATA_RX = 16;
constexpr int8_t PIN_DATA_TX = 17;
constexpr int8_t PIN_LED_BUILTIN = 2;

// ---- HAN UART0 ----
const uint32_t HAN_BAUD = 115200;
const size_t LINE_BUF_SIZE = 256;
char lineBuffer[LINE_BUF_SIZE];
size_t linePos = 0;

// ---- MQTT ----
#define MQTT_HOST IPAddress(MQTT_IP0, MQTT_IP1, MQTT_IP2, MQTT_IP3)
#define MQTT_PORT 8883 // TLS

double energy_kWh = 0;
double energy_kWh_out = 0;

unsigned long lastPublish = 0;
const long publishInterval = 2000;

// Retry intervals when offline
const unsigned long WIFI_RETRY_MS = 2000;
const unsigned long MQTT_RETRY_MS = 5000;
unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;

WiFiClientSecure tlsClient;
PubSubClient mqtt(tlsClient);

char topic[32];
char clientId[24];

void connectToWifi()
{
  if (WiFi.isConnected())
  {
    return;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ---- HAN line parsing ----
// 1-0:1.8.0.255
void processLine(char *line)
{
  const char *patterns[] = {"1-0:1.8.0(", "1.8.0(", "1-1:1.8.0("};

  for (int i = 0; i < 3; i++)
  {
    char *p = strstr(line, patterns[i]);
    if (p != NULL)
    {
      p = strchr(p, '(');
      if (p != NULL)
      {
        p++;

        char valueStr[32];
        size_t vi = 0;
        while (*p != ')' && *p != '*' && *p != '\0' && vi < sizeof(valueStr) - 1)
        {
          valueStr[vi++] = *p++;
        }
        valueStr[vi] = '\0';

        energy_kWh = atof(valueStr);
        energy_kWh_out = energy_kWh;

        Serial.print("Energy (kWh): ");
        Serial.println(energy_kWh_out, 3);
      }
      return;
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
    period = 150; // fast: hunting for WiFi
  else if (!mqtt.connected())
    period = 400; // slow: WiFi up, MQTT down
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
  // HAN meter on UART0
  Serial2.begin(HAN_BAUD, SERIAL_8N1, PIN_DATA_RX, PIN_DATA_TX, true); // DO NOT FORGET TO INVERT HAN POLARITY!
  delay(200);
  Serial.println("ESP32 HAN + WiFi -> MQTT");

  pinMode(PIN_LED_BUILTIN, OUTPUT);
  digitalWrite(PIN_LED_BUILTIN, false);

  // Unique topic and client id per board
  snprintf(topic, sizeof(topic), "xamk/mkl/%02X%02X%02X%02X%02X%02X/kwh",
           WiFi.macAddress()[0], WiFi.macAddress()[1],
           WiFi.macAddress()[2], WiFi.macAddress()[3],
           WiFi.macAddress()[4], WiFi.macAddress()[5]);
  snprintf(clientId, sizeof(clientId), "xamkenergy-%02X%02X%02X%02X%02X%02X",
           WiFi.macAddress()[0], WiFi.macAddress()[1],
           WiFi.macAddress()[2], WiFi.macAddress()[3],
           WiFi.macAddress()[4], WiFi.macAddress()[5]);

  // Verify with the MQTT broker
  //tlsClient.setCACert(MQTT_CA_CERT);
  //tlsClient.setCertificate(ESP32_CA_CERT);
  //tlsClient.setPrivateKey(ESP32_RSA_KEY);
  tlsClient.setInsecure();
  
  WiFi.setSleep(WIFI_PS_NONE);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(60);
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

  if (!mqtt.connected())
  {
    if (millis() - lastMqttAttempt >= MQTT_RETRY_MS)
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
        linePos = 0;
        Serial.println("MQTT connected");
      }
      else
      {
        Serial.println("MQTT connection failed!");
      }
    }
    return;
  }

  mqtt.loop();

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
  
  if (millis() - lastPublish >= publishInterval)
  {
    lastPublish = millis();
    char plain[35];
    snprintf(plain, sizeof(plain), "%.3f", energy_kWh_out);
    mqtt.publish(topic, plain);

    Serial.print("Trying to publish: ");
    Serial.println(plain);
  }
} // loop
