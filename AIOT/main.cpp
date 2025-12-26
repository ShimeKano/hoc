#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// --- CẤU HÌNH HỆ THỐNG ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Phải khớp hoàn toàn với app.py trên Hugging Face
const char* mqtt_server = "test.mosquitto.org"; 
const char* mqtt_topic = "balenkano_tuan_door_2025"; 

WiFiClient espClient;
PubSubClient client(espClient);
Servo myServo;

const int servoPin = 2; // Chân tín hiệu Servo

void setup_wifi() {
  delay(10);
  Serial.println("\n--- ĐANG KẾT NỐI WIFI ---");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi đã sẵn sàng!");
}

// Hàm xử lý khi nhận lệnh từ Hugging Face
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("\n[MQTT] Lệnh mới: ");
  Serial.println(message);

  if (message == "OPEN") {
    Serial.println(">>> XÁC NHẬN CHỦ NHÀ: ĐANG MỞ CỬA...");
    myServo.write(90);  // Quay Servo mở khóa
    delay(30000);        // Giữ cửa mở trong 30 giây
    myServo.write(0);   // Tự động đóng cửa
    Serial.println(">>> CỬA ĐÃ ĐÓNG LẠI.");
  } 
  else if (message == "LOCK") {
    Serial.println(">>> PHÁT HIỆN NGƯỜI LẠ: TIẾP TỤC KHÓA!");
    myServo.write(0);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Đang kết nối MQTT Mosquitto...");
    
    // Tạo ID ngẫu nhiên để tránh bị ngắt kết nối
    String clientId = "ESP32_Tuan_";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("THÀNH CÔNG!");
      client.subscribe(mqtt_topic);
      Serial.println("Đang đợi tín hiệu từ Hugging Face...");
    } else {
      Serial.print("Thất bại, mã lỗi=");
      Serial.print(client.state());
      Serial.println(" - Thử lại sau 5 giây");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Khởi tạo Servo
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2400);
  myServo.write(0); // Cửa đóng mặc định
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}