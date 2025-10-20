#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include "secrets.h" // đặt ở thư mục include/

#define DHTPIN 4
#define DHTTYPE DHT22
#define LEDPIN 17

DHT dht(DHTPIN, DHTTYPE);

void connectWiFi() {
  Serial.print("WiFi connecting to "); Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect timeout");
  }
}

bool httpPostJson(const String& url, const String& body, String& respOut) {
  WiFiClientSecure client;
  client.setInsecure(); // Dev Tunnels: đơn giản hoá chứng chỉ
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", API_KEY);
  int code = http.POST(body);
  respOut = http.getString();
  http.end();
  Serial.printf("POST %s => %d\n", url.c_str(), code);
  return code >= 200 && code < 300;
}

bool httpGet(const String& url, String& respOut) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.addHeader("X-API-KEY", API_KEY);
  int code = http.GET();
  respOut = http.getString();
  http.end();
  Serial.printf("GET %s => %d\n", url.c_str(), code);
  return code >= 200 && code < 300;
}

void sendReading(float t, float h) {
  StaticJsonDocument<200> doc;
  doc["temperature"] = t;
  doc["humidity"] = h;
  String body; serializeJson(doc, body);
  String resp;
  if (!httpPostJson(String(API_BASE) + "/readings", body, resp)) {
    Serial.println("sendReading failed: " + resp);
  } else {
    Serial.println("sendReading ok: " + resp);
  }
}

void applyLedState(int state) {
  digitalWrite(LEDPIN, state ? HIGH : LOW);
}

void pollLed() {
  String resp;
  if (!httpGet(String(API_BASE) + "/led-state", resp)) {
    Serial.println("pollLed failed: " + resp);
    return;
  }
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) { Serial.println("JSON parse err: " + String(err.c_str())); return; }

  if (doc.containsKey("commands")) {
    for (JsonObject cmd : doc["commands"].as<JsonArray>()) {
      const char* type = cmd["command_type"] | "";
      if (String(type) == "led") {
        int state = cmd["payload"]["state"] | -1;
        if (state == 0 || state == 1) {
          applyLedState(state);
          Serial.printf("LED set by command => %d\n", state);
        }
      }
    }
  } else if (!doc["state"].isNull()) {
    int state = doc["state"];
    applyLedState(state);
    Serial.printf("LED set to last state => %d\n", state);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LEDPIN, OUTPUT);
  dht.begin();
  connectWiFi();
}

unsigned long lastSend = 0;
unsigned long lastPoll = 0;

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(1000);
    return;
  }

  unsigned long now = millis();

  // Gửi dữ liệu DHT mỗi 10s
  if (now - lastSend > 10000) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      sendReading(t, h);
    } else {
      Serial.println("DHT read failed.");
    }
    lastSend = now;
  }

  // Poll lệnh LED mỗi 5s
  if (now - lastPoll > 5000) {
    pollLed();
    lastPoll = now;
  }

  delay(200);
}