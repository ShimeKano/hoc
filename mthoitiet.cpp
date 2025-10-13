#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi (Wokwi)
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// OpenWeatherMap
const char* API_KEY = "c4a5fc38ca36f1246e5fa7e771f987d7";
const char* CITY = "Vinh Long";
const char* COUNTRY_CODE = "VN";
const char* UNITS = "metric"; // "metric" => °C
const char* LANG = "vi";      // Tiếng Việt

// LCD I2C: địa chỉ thường là 0x27, kích thước 20x4
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Tần suất cập nhật (ms)
const uint32_t REFRESH_INTERVAL = 60 * 1000; // 60 giây (demo). Nên tăng lên 10 phút để tránh giới hạn API.

uint8_t findI2CAddress() {
  byte error, address;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      return address;
    }
  }
  return 0; // không tìm thấy
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Dang ket noi WiFi");
  lcd.setCursor(0, 1); lcd.print(WIFI_SSID);

  Serial.print("Connecting to WiFi ");
  Serial.print(WIFI_SSID);
  int dot = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(dot % 20, 2);
    lcd.print(".");
    dot++;
  }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("WiFi OK: ");
  lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());
}

String urlEncode(const String &s) {
  String enc = "";
  char c;
  char buf[4];
  for (size_t i = 0; i < s.length(); i++) {
    c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      enc += c;
    } else {
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      enc += buf;
    }
  }
  return enc;
}

String buildUrl() {
  // Dùng q=City,CountryCode cho đơn giản. Có thể thay bằng lat/lon nếu muốn chính xác.
  String q = urlEncode(String(CITY) + "," + COUNTRY_CODE);
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + q +
               "&appid=" + API_KEY +
               "&units=" + UNITS +
               "&lang=" + LANG;
  return url;
}

bool fetchWeather(String &payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return false;
  }

  WiFiClient client;  // HTTP (không SSL)
  HTTPClient http;
  String url = buildUrl();
  Serial.println("GET " + url);

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode <= 0) {
    Serial.printf("HTTP GET failed: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  Serial.printf("HTTP code: %d\n", httpCode);
  if (httpCode == HTTP_CODE_OK) {
    payload = http.getString();
    http.end();
    return true;
  } else {
    Serial.println("Unexpected HTTP code");
    http.end();
    return false;
  }
}

String fit20(const String &s) {
  if (s.length() <= 20) return s;
  return s.substring(0, 20);
}

void showBootScreen(uint8_t i2cAddr) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("ESP32 + LCD 20x4");
  lcd.setCursor(0, 1); lcd.print("I2C Addr: 0x");
  lcd.print(i2cAddr, HEX);
  lcd.setCursor(0, 2); lcd.print("OpenWeatherMap");
  lcd.setCursor(0, 3); lcd.print("Khoi dong...");
}

bool parseAndDisplay(const String &json) {
  StaticJsonDocument<2048> doc; // đủ cho /weather
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("JSON loi: ");
    lcd.setCursor(0, 1); lcd.print(fit20(String(err.c_str())));
    return false;
  }

  const char* name = doc["name"] | "Vinh Long";
  const char* country = doc["sys"]["country"] | "VN";
  float temp = doc["main"]["temp"] | NAN;
  float feels = doc["main"]["feels_like"] | NAN;
  int humidity = doc["main"]["humidity"] | -1;
  const char* desc = doc["weather"][0]["description"] | "";

  Serial.println("City: " + String(name) + ", " + String(country));
  Serial.printf("Temp: %.1f C, Feels: %.1f C, H: %d%%\n", temp, feels, humidity);
  Serial.println("Desc: " + String(desc));

  // Hiển thị LCD 20x4
  lcd.clear();
  // Dòng 0: địa điểm
  lcd.setCursor(0, 0); lcd.print(fit20(String(name) + " " + country));
  // Dòng 1: nhiệt độ + độ ẩm
  // Ví dụ: "T:28.4C  H:67%"
  char line1[21];
  snprintf(line1, sizeof(line1), "T:%.1fC  H:%d%%", temp, humidity);
  lcd.setCursor(0, 1); lcd.print(line1);
  // Dòng 2: mô tả (vi)
  lcd.setCursor(0, 2); lcd.print(fit20(String(desc)));
  // Dòng 3: Cảm giác
  char line3[21];
  snprintf(line3, sizeof(line3), "Cam giac: %.1fC", feels);
  lcd.setCursor(0, 3); lcd.print(line3);

  return true;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); // SDA=21, SCL=22 mặc định trên ESP32 core
  delay(100);

  // Nếu không chắc địa chỉ LCD, quét I2C
  uint8_t addr = findI2CAddress();
  if (addr != 0) {
    // Re-init LCD với địa chỉ tìm được
    // Cảnh báo: Một số lib không cho đổi địa chỉ sau khi tạo đối tượng.
    // Trường hợp này ta tạo lại đối tượng tạm để init và dùng luôn lcd cũ nếu cùng addr.
  }

  lcd.init();
  lcd.backlight();

  showBootScreen(0x27);

  connectWiFi();
}

void loop() {
  static uint32_t lastFetch = 0;
  if (millis() - lastFetch >= REFRESH_INTERVAL || lastFetch == 0) {
    lastFetch = millis();

    String payload;
    if (fetchWeather(payload)) {
      parseAndDisplay(payload);
    } else {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("HTTP loi/Khong net");
      lcd.setCursor(0, 1); lcd.print("Kiem tra WiFi/API");
    }
  }
  delay(100);
}