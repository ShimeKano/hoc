#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// ==== LCD I2C ====
#define I2C_ADDR 0x27
#define LCD_COLUMNS 20
#define LCD_LINES 4
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);

// ==== WiFi ====
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ==== API URLs ====
String apiGold = "http://api.btmc.vn/api/BTMCAPI/getpricebtmc?key=3kd8ub1llcg9t45hnoh8hmn7t5kc2v";
String apiFuel = "https://wifeed.vn/api/du-lieu-vimo/hang-hoa/gia-xang-dau-trong-nuoc?page=1&limit=100&apikey=demo";
String apiIP   = "https://ipinfo.io/json";

// ==== Biến dữ liệu ====
String goldBuy = "null", goldSell = "null";
String fuel92 = "null", fuel95 = "null";
String myIP = "null", myCity = "null", myCountry = "null";

// ==== Bộ đếm để luân phiên màn hình ====
int screenIndex = 0;       // 0: vàng, 1: xăng, 2: mạng
unsigned long lastSwitch = 0;
const unsigned long screenDelay = 5000; // 5 giây chuyển màn

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Dang ket noi WiFi...");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("Da ket noi WiFi!");
  delay(1000);

  // Lấy dữ liệu ban đầu
  getGoldData();
  getFuelData();
  getIPData();
  showScreen(screenIndex); // Hiển thị lần đầu
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.print("WiFi mat ket noi!");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  // Cập nhật dữ liệu định kỳ mỗi 1 phút
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 60000) {
    getGoldData();
    getFuelData();
    getIPData();
    lastUpdate = millis();
  }

  // Luân phiên hiển thị 3 màn hình
  if (millis() - lastSwitch > screenDelay) {
    screenIndex = (screenIndex + 1) % 3;
    showScreen(screenIndex);
    lastSwitch = millis();
  }
}

// === Màn hình hiển thị ===
void showScreen(int index) {
  lcd.clear();
  switch (index) {
    case 0:
      lcd.setCursor(0,0);
      lcd.print("== GIA VANG BTMC ==");
      lcd.setCursor(0,1);
      lcd.print("Mua : " + goldBuy);
      lcd.setCursor(0,2);
      lcd.print("Ban : " + goldSell);
      lcd.setCursor(0,3);
      lcd.print("Cap nhat moi nhat");
      break;

    case 1:
      lcd.setCursor(0,0);
      lcd.print("== GIA XANG DAU ==");
      lcd.setCursor(0,1);
      lcd.print("RON92: " + fuel92 + "k");
      lcd.setCursor(0,2);
      lcd.print("RON95: " + fuel95 + "k");
      lcd.setCursor(0,3);
      lcd.print("Nguon: wifeed.vn");
      break;

    case 2:
      lcd.setCursor(0,0);
      lcd.print("== THONG TIN MANG ==");
      lcd.setCursor(0,1);
      lcd.print("IP: " + myIP);
      lcd.setCursor(0,2);
      lcd.print("QG: " + myCountry);
      lcd.setCursor(0,3);
      lcd.print("TP: " + myCity);
      break;
  }
}

// === Lấy dữ liệu Giá Vàng ===
void getGoldData() {
  HTTPClient http;
  http.begin(apiGold);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, payload);
    JsonObject goldItem = doc["DataList"]["Data"][3];
    goldBuy  = goldItem["@pb_4"].as<String>();
    goldSell = goldItem["@ps_4"].as<String>();
  }
  http.end();
}

// === Lấy dữ liệu Giá Xăng ===
void getFuelData() {
  HTTPClient http;
  http.begin(apiFuel);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("=== FUEL DATA ===");
    Serial.println(payload);

    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.print("JSON error: ");
      Serial.println(err.f_str());
      return;
    }

    JsonObject fuelItem = doc["data"][0];

    // Lấy giá xăng RON92 và RON95 (vùng 1)
    if (fuelItem.containsKey("vung_1_xang_sinh_hoc_e5_ron_92_ii"))
      fuel92 = String((float)fuelItem["vung_1_xang_sinh_hoc_e5_ron_92_ii"]);
    else
      fuel92 = "N/A";

    if (fuelItem.containsKey("vung_1_xang_ron_95_ii_iii"))
      fuel95 = String((float)fuelItem["vung_1_xang_ron_95_ii_iii"]);
    else
      fuel95 = "N/A";

    Serial.print("RON92 = "); Serial.println(fuel92);
    Serial.print("RON95 = "); Serial.println(fuel95);
  } else {
    Serial.print("Loi HTTP fuel: ");
    Serial.println(httpCode);
  }
  http.end();
}


// === Lấy dữ liệu IP / Thành phố / Quốc gia ===
void getIPData() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, apiIP);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(512);
    deserializeJson(doc, payload);
    myIP = doc["ip"].as<String>();
    myCity = doc["city"].as<String>();
    myCountry = doc["country"].as<String>();
  }
  http.end();
}