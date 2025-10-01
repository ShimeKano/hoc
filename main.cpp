#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

#define DHTPIN 18      // GPIO18 cho DHT22 (DATA)
#define DHTTYPE DHT22

// WiFi & MQTT
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";

// MQTT topics
const char* TOPIC_TEMP      = "tuanprohl/temp";
const char* TOPIC_HUM       = "tuanprohl/hum";
const char* TOPIC_TEMP_AVG  = "tuanprohl/temp_avg";
const char* TOPIC_HUM_AVG   = "tuanprohl/hum_avg";
const char* TOPIC_LCD_TEXT  = "tuanprohl/lcd_text"; // topic nhận text từ web

// LCD I2C
static const uint8_t LCD_ADDR = 0x27; // Wokwi LCD2004 I2C mặc định
static const uint8_t LCD_COLS = 20;
static const uint8_t LCD_ROWS = 4;

// Giới hạn độ dài dòng để tránh tốn RAM (có thể tăng nếu cần)
static const uint16_t MAX_LINE_LEN = 128;

// Objects
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// DHT tracking
float prevTemp = NAN;
float prevHum  = NAN;
float lastTemps[3] = {NAN, NAN, NAN};
float lastHums[3]  = {NAN, NAN, NAN};

// LCD scrolling state
String lcdRaw[LCD_ROWS] = {"", "", "", ""};   // nội dung thật của từng dòng (không cắt)
uint16_t lcdLen[LCD_ROWS] = {0, 0, 0, 0};     // độ dài từng dòng
uint16_t lcdOffset[LCD_ROWS] = {0, 0, 0, 0};  // vị trí scroll hiện tại cho từng dòng
unsigned long lastScrollMs = 0;
const unsigned long SCROLL_INTERVAL_MS = 350; // tốc độ cuộn (ms)

// Tạo cửa sổ hiển thị 20 ký tự với offset, có wrap về đầu
String window20(const String& s, uint8_t width, uint16_t offset) {
  uint16_t L = s.length();
  if (L == 0) return String(' ', width);
  if (L <= width) {
    String out = s;
    while (out.length() < width) out += ' ';
    return out;
  }
  String out; out.reserve(width);
  for (uint8_t i = 0; i < width; i++) {
    out += s[(offset + i) % L];
  }
  return out;
}

void renderLcdAll() {
  for (uint8_t r = 0; r < LCD_ROWS; r++) {
    String view = window20(lcdRaw[r], LCD_COLS, lcdOffset[r]);
    lcd.setCursor(0, r);
    lcd.print(view);
  }
}

void scrollTask() {
  unsigned long now = millis();
  if (now - lastScrollMs < SCROLL_INTERVAL_MS) return;
  lastScrollMs = now;

  bool anyScrolled = false;
  for (uint8_t r = 0; r < LCD_ROWS; r++) {
    if (lcdLen[r] > LCD_COLS) {
      lcdOffset[r] = (lcdOffset[r] + 1) % lcdLen[r];
      anyScrolled = true;
    }
  }
  if (anyScrolled) {
    renderLcdAll();
  }
}

// ---- WiFi/MQTT ----
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

void handleLcdPayload(const String& data) {
  Serial.println("📥 LCD payload nhận được:");
  Serial.println(data);

  // Tách tối đa 4 dòng theo '\n'
  String lines[LCD_ROWS];
  uint8_t row = 0;
  String cur = "";
  for (unsigned int i = 0; i < data.length() && row < LCD_ROWS; i++) {
    char c = data[i];
    if (c == '\r') continue; // bỏ CR
    if (c == '\n') {
      lines[row++] = cur;
      cur = "";
      if (row >= LCD_ROWS) break;
    } else {
      cur += c;
    }
  }
  if (row < LCD_ROWS) {
    lines[row++] = cur;
    while (row < LCD_ROWS) lines[row++] = "";
  }

  // Lưu vào trạng thái scrolling, cắt trần ở MAX_LINE_LEN để tránh quá dài
  for (uint8_t r = 0; r < LCD_ROWS; r++) {
    if (lines[r].length() > MAX_LINE_LEN) {
      lcdRaw[r] = lines[r].substring(0, MAX_LINE_LEN);
    } else {
      lcdRaw[r] = lines[r];
    }
    lcdLen[r] = lcdRaw[r].length();
    lcdOffset[r] = 0; // reset offset khi có nội dung mới
  }

  // In khung ban đầu
  renderLcdAll();
  Serial.println("🖨️ Đã in lên LCD 20x4 (cuộn nếu > 20 ký tự).");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String tpc = String(topic);
  String data; data.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    data += (char)payload[i];
  }

  if (tpc == TOPIC_LCD_TEXT) {
    handleLcdPayload(data);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Đang kết nối MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("✅ Đã kết nối!");
      client.subscribe(TOPIC_LCD_TEXT);
      Serial.print("🔔 Subscribed: ");
      Serial.println(TOPIC_LCD_TEXT);
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

  // DHT
  dht.begin();

  // WiFi + MQTT
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  // I2C + LCD (ESP32: chỉ định SDA=21, SCL=22 để khớp sơ đồ)
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 LCD 20x4");
  lcd.setCursor(0, 1);
  lcd.print("Waiting for text");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Cuộn LCD nếu cần
  scrollTask();

  // Đọc DHT22 và publish nếu thay đổi >= 5 đơn vị
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("⚠️ Lỗi đọc DHT22");
    delay(2000);
    return;
  }

  bool tempChanged = isnan(prevTemp) || fabs(t - prevTemp) >= 5.0;
  bool humChanged  = isnan(prevHum)  || fabs(h - prevHum)  >= 5.0;

  if (tempChanged) {
    String tempStr = String(t, 1);
    client.publish(TOPIC_TEMP, tempStr.c_str());
    Serial.print("📤 Nhiệt độ gửi: ");
    Serial.println(tempStr);

    // Cập nhật mảng 3 lần gần nhất (nhiệt độ)
    lastTemps[2] = lastTemps[1];
    lastTemps[1] = lastTemps[0];
    lastTemps[0] = t;

    // Tính trung bình nếu đã có đủ 3 lần
    if (!isnan(lastTemps[0]) && !isnan(lastTemps[1]) && !isnan(lastTemps[2])) {
      float avgTemp = (lastTemps[0] + lastTemps[1] + lastTemps[2]) / 3.0;
      String avgStr = String(avgTemp, 1);
      client.publish(TOPIC_TEMP_AVG, avgStr.c_str());
    }

    prevTemp = t;
  }

  if (humChanged) {
    String humStr = String(h, 1);
    client.publish(TOPIC_HUM, humStr.c_str());
    Serial.print("📤 Độ ẩm gửi: ");
    Serial.println(humStr);

    // Cập nhật mảng 3 lần gần nhất (độ ẩm)
    lastHums[2] = lastHums[1];
    lastHums[1] = lastHums[0];
    lastHums[0] = h;

    // Tính trung bình nếu đã có đủ 3 lần
    if (!isnan(lastHums[0]) && !isnan(lastHums[1]) && !isnan(lastHums[2])) {
      float avgHum = (lastHums[0] + lastHums[1] + lastHums[2]) / 3.0;
      String avgStr = String(avgHum, 1);
      client.publish(TOPIC_HUM_AVG, avgStr.c_str());
    }

    prevHum = h;
  }

  delay(30); // giữ vòng lặp mượt, scrollTask dùng millis nên không phụ thuộc delay lớn
}