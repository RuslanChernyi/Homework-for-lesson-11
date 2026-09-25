#include <Arduino.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "secrets.h"

/***    Defines   ***/

#define BUTTON_LOGIC_HIGH 0
#define BUTTON_LOGIC_LOW 1

#define STATUS_OK 0b00000000
#define STATUS_LDR_ERR 0b00000001  // біт 0: LDR помилка
#define STATUS_DHT_ERR 0b00000010  // біт 1: DHT22 помилка
#define STATUS_WIFI_ERR 0b00000100 // біт 2: Wi-Fi помилка

// WIFI settings
#define WIFI_SSID     "Wokwi-GUEST"  // мережа Wokwi симулятора
#define WIFI_PASSWORD ""              // без пароля
#define WIFI_TIMEOUT  10000           // максимум 10 секунд на підключення

// MQTT settings
#define DOMAIN_ADDRESS "a1u0a7gycxitea-ats.iot.eu-central-1.amazonaws.com"
#define MQTT_PORT 8883
#define MQTT_CLIENT_ID "esp32-Ruslan_Chernyi"
#define TOPIC_SENSORS "iot-course/Ruslan_Chernyi/sensors/data"
#define TOPIC_BUTTON "iot-course/Ruslan_Chernyi/commands"
#define DEVICE_PAYLOAD_ID "ESP32_DHT22"

// Timer intervals
#define PUBLISH_INTERVAL 10000
#define RECONNECT_INTERVAL 5000
#define DEBOUNCE_DELAY 30
#define MANUAL_PUBLISH_DELAY 200

//  Pin defines
#define SWITCH_PIN 5
#define SENSOR_DHT_PIN 4
#define DHTT_TYPE DHT22

/***    Structs           ***/
typedef struct {
  float humidity;
  float temperature;
  uint8_t status;
} s_dhtData;

/***    Global variables    ***/
uint32_t lastReconectionAttempt = 0;
uint32_t reconectionAttempts = 0;
uint32_t lastPublish = 0;
uint8_t g_buttonState = BUTTON_LOGIC_LOW;

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht(SENSOR_DHT_PIN, DHTT_TYPE);

// put function declarations here:
bool connectWifi(void);
void configureTLS(void);
bool connectMqtt(void);
void syncNtpTime(void);
void checkButton(uint8_t buttonPin);
void publishOnDemand_manual(void);
bool isWifiConnected(void);
s_dhtData get_dhtData(void);
void publishSensorData(float temperature, float humidity);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Старт");
  pinMode(SWITCH_PIN,INPUT_PULLUP);

  dht.begin();
  connectWifi();
  if (!isWifiConnected()) {
    Serial.println("Could not connect to WiFi");
    Serial.println("Reconnecting...");
    connectWifi();
  }
  configureTLS();
  syncNtpTime();
  mqttClient.setServer(DOMAIN_ADDRESS, MQTT_PORT);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);
  connectMqtt();
}

void loop() {
  s_dhtData dht_data = get_dhtData();

  if (mqttClient.connected()) {
    mqttClient.loop();
    uint32_t now = millis();
    reconectionAttempts = 0;
    if ((now - lastPublish) > PUBLISH_INTERVAL) {
      lastPublish = now;
      if((dht_data.status & STATUS_DHT_ERR) == STATUS_OK){
        publishSensorData(dht_data.temperature, dht_data.humidity);
      }
    }
    checkButton(SWITCH_PIN);
    static uint8_t manual_published = 0;

    // button not checked during disconnect - no broker to publish to
    if (g_buttonState == BUTTON_LOGIC_HIGH) {
      // Write Manual_Send on button push
      if (manual_published == 0) {
        // publish first manual topic
        publishOnDemand_manual();
        manual_published++;
      }
    } else {
      manual_published = 0;
    }
  } else {
    uint32_t now = millis();
    if ((now - lastReconectionAttempt) > RECONNECT_INTERVAL) {
      lastReconectionAttempt = now;
      if (reconectionAttempts == 3) {
        Serial.println("[MQTT] Неможливо підключитись.");
      } else if (reconectionAttempts > 3) {
      } else {
        Serial.println("[MQTT] З'єднання втрачено - перепідключаємось...");
        connectMqtt();
      }
      reconectionAttempts++;
   }
  }
}

// put function definitions here:

bool connectWifi() {
  ;
  Serial.println("Connecting to Wifi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT) {
      Serial.println("timeout");
      return false;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println(" OK");
  Serial.println("WiFi IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool connectMqtt() {
  Serial.print("[MQTT] Підключаємось до ");
  Serial.print(DOMAIN_ADDRESS);
  Serial.print("...");

  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    Serial.println("OK");
    return true;
  }

  Serial.print(" помилка: ");
  Serial.println(mqttClient.state());
  return false;
}

void publishSensorData(float temperature, float humidity) {
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] не підключено - пропускаємо");
    return;
  }
  char payload[140];
  snprintf(payload, sizeof(payload),
           "{\"device_id\": \"%s\",\"temperature\": %.1f, \"humidity\": %.1f,\"timestamp\": %ld}",DEVICE_PAYLOAD_ID, temperature,
           humidity, (long)time(nullptr));
  Serial.print("[MQTT] Публікуємо: ");
  Serial.println(payload);

  bool publish_status = mqttClient.publish(TOPIC_SENSORS, payload);
  Serial.println(publish_status ? "[MQTT] OK" : "[MQTT] Помилка публікації");
}

void publishOnDemand_manual(void) {
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] не підключено - пропускаємо");
    return;
  }
  char payload[20];
  snprintf(payload, sizeof(payload), "\"manual_read\"");

  bool publish_status = mqttClient.publish(TOPIC_BUTTON, payload);
  Serial.println(publish_status ? "[MQTT] OK" : "[MQTT] Помилка публікації");
}

bool isWifiConnected() { return WiFi.status() == WL_CONNECTED; }

s_dhtData get_dhtData(void) {
  s_dhtData data = {0};
  // Get the sensor values
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temp) && !isnan(humidity)) {
    data.status = STATUS_OK;
    data.humidity = humidity;
    data.temperature = temp;
  } else {
    data.status |= STATUS_DHT_ERR;
    data.humidity = 0.0f;
    data.temperature = 0.0f;
  }
  return data;
}

void checkButton(uint8_t buttonPin) {

  static uint8_t currentSavedButtonState = digitalRead(buttonPin);
  static uint32_t debounceStartTime = 0;
  static uint8_t debounceStarted = 0;

  uint8_t buttonState = digitalRead(buttonPin);

  // Detect state transition
  if (buttonState != currentSavedButtonState && !debounceStarted) {
    debounceStartTime = millis();
    debounceStarted = 1;
  }

  // Check the state again after the debounce delay
  if (debounceStarted && (millis() - debounceStartTime >= DEBOUNCE_DELAY)) {
    if (buttonState != currentSavedButtonState) {
      currentSavedButtonState = buttonState;
      // Send message to the queue if the button state is logic high
      if (buttonState == BUTTON_LOGIC_HIGH) {
        g_buttonState = BUTTON_LOGIC_HIGH;
        Serial.println("\nButton was pressed");
      } else {
        g_buttonState = BUTTON_LOGIC_LOW;
      }
    }
    // Reset debounce flag regardless of press or release
    debounceStarted = 0;
  }
  return;
}

void configureTLS(void){
 wifiClient.setCACert(root_ca);
 wifiClient.setCertificate(device_cert);
 wifiClient.setPrivateKey(private_key);
}

void syncNtpTime(void){
  configTime(0, 0, "pool.ntp.org");
  Serial.print("Syncing time");
  while (time(nullptr) < 100000) { // still near-zero = not synced yet
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Time synced"); 
}