#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>

// WiFi (có thể để trống nếu không cần IP)
static const char* WIFI_SSID = "Wokwi-GUEST";
static const char* WIFI_PASS = "";

// LCD I2C 20x4 (đổi 0x27 <-> 0x3F nếu cần)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// CHỈNH GIÁ Ở ĐÂY
// Ví dụ tham khảo thị trường (bạn thay số cho phù hợp)
static const char* GAS_LINE   = "Xang RON95: 25,640d";   // <= 20 ký tự
static const char* GOLD_LINE  = "Vang: 2,350 USD/oz";    // <= 20 ký tự
static const char* GEO_LINE   = "VN, Vinh Long";         // <= 20 ký tự

String fit20(const String& s) {
  return s.length() <= 20 ? s : s.substring(0, 20);
}

void show4(const String& l0, const String& l1, const String& l2, const String& l3) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(fit20(l0));
  lcd.setCursor(0,1); lcd.print(fit20(l1));
  lcd.setCursor(0,2); lcd.print(fit20(l2));
  lcd.setCursor(0,3); lcd.print(fit20(l3));
}

void connectWiFiIfNeeded(String& ipOut) {
  ipOut = "No WiFi";
  if (strlen(WIFI_SSID) == 0) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
    delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    ipOut = WiFi.localIP().toString();
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); // SDA=21, SCL=22
  lcd.init();
  lcd.backlight();

  String ip;
  connectWiFiIfNeeded(ip);

  // In 4 dòng cố định
  String l0 = GAS_LINE;              // Giá xăng
  String l1 = GOLD_LINE;             // Giá vàng
  String l2 = "IP: " + ip;           // IP cục bộ (hoặc No WiFi)
  String l3 = GEO_LINE;              // Quốc gia, Thành phố (tự đặt)

  show4(l0, l1, l2, l3);

  // Nếu muốn cập nhật theo chu kỳ (vẫn giá cứng), bỏ comment khối loop bên dưới
}

void loop() {
  // Không cần làm gì thêm vì đang in giá cứng.
  // Nếu muốn luân phiên các màn hình, bạn có thể thêm code ở đây.
  delay(1000);
}