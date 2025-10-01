#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

#define DHTPIN 18      // GPIO18
#define DHTTYPE DHT22  // DHT22

DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "Wokwi-GUEST";      // WiFi của Wokwi
const char* password = "";             // không cần pass
const char* mqtt_server = "broker.emqx.io"; // Broker cũ

WiFiClient espClient;
PubSubClient client(espClient);

float prevTemp = NAN;
float prevHum = NAN;
float lastTemps[3] = {NAN, NAN, NAN};

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Đang kết nối WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n  ✅ WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Đang kết nối MQTT...");
    // Client ID ngẫu nhiên tránh bị trùng
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("✅ Đã kết nối!");
    } else {
      Serial.print("❌ failed, rc=");
      Serial.print(client.state());
      Serial.println(" thử lại sau 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("⚠️ Lỗi đọc DHT22");
    delay(2000);
    return;
  }

  bool tempChanged = isnan(prevTemp) || abs(t - prevTemp) >= 5.0;
  bool humChanged  = isnan(prevHum) || abs(h - prevHum) >= 5.0;

  if (tempChanged) {
    String tempStr = String(t, 1);
    client.publish("tuanprohl/temp", tempStr.c_str());
    Serial.print("📤 Nhiệt độ gửi: ");
    Serial.println(tempStr);
    
     // Cập nhật mảng 3 lần gần nhất
    lastTemps[2] = lastTemps[1];
    lastTemps[1] = lastTemps[0];
    lastTemps[0] = t;

    // Tính trung bình nếu đã có đủ 3 lần
    if (!isnan(lastTemps[0]) && !isnan(lastTemps[1]) && !isnan(lastTemps[2])) {
      float avgTemp = (lastTemps[0] + lastTemps[1] + lastTemps[2]) / 3.0;
      Serial.print("🌡️ Trung bình 3 lần gần nhất: ");
      Serial.println(avgTemp, 1);

      // Nếu muốn gửi MQTT
      String avgStr = String(avgTemp, 1);
      client.publish("tuanprohl/temp_avg", avgStr.c_str());
    }

    prevTemp = t;
  }

  if (humChanged) {
    String humStr = String(h, 1);
    client.publish("tuanprohl/hum", humStr.c_str());
    Serial.print("📤 Độ ẩm gửi: ");
    Serial.println(humStr);
    prevHum = h;
  }

  delay(2000);
}
