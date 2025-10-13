#include <Arduino.h>
#include <WiFi.h>
#include <ThingSpeak.h>
#include <DHT.h>
#include "secrets.h"

// Cảm biến DHT22 cấu hình
#define DHT_PIN   19     // CHÂN DATA DHT22 nối GPIO 19
#define DHT_TYPE  DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// ThingSpeak
WiFiClient tsClient;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_SECONDS = 20; // >= 15s theo ThingSpeak free

// Kết nối WiFi
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Dang ket noi WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK - IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi that bai (timeout).");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  dht.begin();
  delay(1500); // Cho DHT ổn định

  connectWiFi();
  ThingSpeak.begin(tsClient);

  Serial.println("Khoi dong xong.");
}

void loop() {
  // Duy trì WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Giới hạn tần suất ghi ThingSpeak
  if (millis() - lastUpdate < UPDATE_SECONDS * 1000UL) {
    delay(50);
    return;
  }
  lastUpdate = millis();

  // Đọc DHT22
  float hum = dht.readHumidity();
  float tmp = dht.readTemperature(); // °C
  if (isnan(hum) || isnan(tmp)) {
    delay(2000); // thử lại một lần
    hum = dht.readHumidity();
    tmp = dht.readTemperature();
  }

  if (isnan(hum) || isnan(tmp)) {
    Serial.println("Doc DHT bi NaN. Bo qua ky nay.");
    return;
  }

  Serial.print("Nhiet do: "); Serial.print(tmp); Serial.print(" °C, ");
  Serial.print("Do am: ");    Serial.print(hum); Serial.println(" %");

  // Ghi Field lên ThingSpeak
  ThingSpeak.setField(1, tmp); // Field1 = Temperature
  ThingSpeak.setField(2, hum); // Field2 = Humidity
  ThingSpeak.setStatus("ESP32 DHT22 OK");

  int httpCode = ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_WRITE_API_KEY);
  if (httpCode == 200) {
    Serial.println("Cap nhat ThingSpeak OK.");
  } else {
    Serial.print("Loi ThingSpeak, ma = ");
    Serial.println(httpCode);
  }
}