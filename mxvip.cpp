#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

// ====== CẤU HÌNH WIFI ======
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ====== API LINKS ======
String API_XANG = "https://wifeed.vn/api/du-lieu-vimo/hang-hoa/gia-xang-dau-trong-nuoc?page=1&limit=5&apikey=demo";
String API_VANG = "http://api.btmc.vn/api/BTMCAPI/getpricebtmc?key=3kd8ub1llcg9t45hnoh8hmn7t5kc2v";
String API_IPINFO = "https://ipinfo.io/json";

LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.print("Dang ket noi WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("WiFi da ket noi!");
  delay(1000);
  lcd.clear();
}
// ================== HÀM LẤY GIÁ XĂNG ==================
void showFuelPrice() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(API_XANG);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println(payload);

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        JsonObject data = doc["data"][0];
        float xang95 = data["vung_1_xang_ron_95_ii_iii"];
        float xang92 = data["vung_2_xang_sinh_hoc_e5_ron_92_ii"];
        float dauhoa = data["vung_1_dau_hoa"];

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("=== GIA XANG DAU ===");
        lcd.setCursor(0, 1);
        lcd.print("RON95: " + String(xang95) + "k");
        lcd.setCursor(0, 2);
        lcd.print("E5RON92: " + String(xang92) + "k");
        lcd.setCursor(0, 3);
        lcd.print("Dau hoa: " + String(dauhoa) + "k");
      } else {
        lcd.clear();
        lcd.print("Loi parse JSON xang");
      }
    } else {
      lcd.clear();
      lcd.print("HTTP xang loi: " + String(httpCode));
    }
    http.end();
  }
  delay(7000);
}

// ================== HÀM LẤY GIÁ VÀNG ==================
void showGoldPrice() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://api.btmc.vn/api/BTMCAPI/getpricebtmc?key=3kd8ub1llcg9t45hnoh8hmn7t5kc2v");
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println(payload);

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        JsonArray data = doc["DataList"]["Data"].as<JsonArray>();

        if (data.size() > 0) {
          // 🟢 Lấy phần tử đầu tiên (vàng miếng BTMC)
          JsonObject item = data[0];

          String name = item["@n_1"] | "";
          String buy = item["@pb_1"] | "";
          String sell = item["@ps_1"] | "";

          Serial.println("Ten: " + name);
          Serial.println("Mua: " + buy);
          Serial.println("Ban: " + sell);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("=== GIA VANG BTMC ===");
          lcd.setCursor(0, 1);
          lcd.print(name.substring(0, 20));
          lcd.setCursor(0, 2);
          lcd.print("Mua: " + buy);
          lcd.setCursor(0, 3);
          lcd.print("Ban: " + sell);
        } else {
          lcd.clear();
          lcd.print("Khong co du lieu vang");
        }
      } else {
        lcd.clear();
        lcd.print("Loi parse JSON vang");
        Serial.println(error.c_str());
      }
    } else {
      lcd.clear();
      lcd.print("HTTP vang loi: " + String(httpCode));
    }
    http.end();
  }
  delay(7000);
}

// ================== HÀM LẤY THÔNG TIN IP ==================
void showIPInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(API_IPINFO);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println(payload);

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        String ip = doc["ip"].as<String>();
        String city = doc["city"].as<String>();
        String country = doc["country"].as<String>();

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("=== THONG TIN MANG ===");
        lcd.setCursor(0, 1);
        lcd.print("IP: " + ip);
        lcd.setCursor(0, 2);
        lcd.print(city + ", " + country);
      } else {
        lcd.clear();
        lcd.print("Loi parse JSON IP");
      }
    } else {
      lcd.clear();
      lcd.print("HTTP IP loi: " + String(httpCode));
    }
    http.end();
  }
  delay(7000);
}

void loop() {
  // ==== 1️⃣ HIỂN THỊ GIÁ XĂNG DẦU ====
  showFuelPrice();

  // ==== 2️⃣ HIỂN THỊ GIÁ VÀNG ====
  showGoldPrice();

  // ==== 3️⃣ HIỂN THỊ IP, QUỐC GIA, THÀNH PHỐ ====
  showIPInfo();

  delay(30000); // cập nhật mỗi 30 giây
}

