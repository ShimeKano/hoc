#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <time.h>

// ====== Cảm biến DHT22 ======
#define DHTPIN 19
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ====== WiFi ======
const char* WIFI_SSID = "Wokwi-GUEST"; // đổi nếu chạy ESP32 thật
const char* WIFI_PASS = "";

// ====== Thiết bị & API ======
const char* DEVICE_ID = "esp32-1";
// Phải trùng với C:\xampp\htdocs\iot_dht\config.php
const char* API_KEY   = "CHANGE_ME_SECRET_32CHARS";

// Domain ngrok HTTP-only của bạn
static const char* NGROK_HOST = "noninherently-pitchable-sparkle.ngrok-free.dev";
static const uint16_t NGROK_HTTP_PORT = 80;

// Đường dẫn API
static const char* PATH_HEALTH = "/iot_dht/api/health.php";
static const char* PATH_INGEST = "/iot_dht/api/ingest.php";
static const char* PATH_THRESH = "/iot_dht/api/thresholds.php";

// ====== Nhịp ======
const unsigned long SAMPLE_INTERVAL_MS   = 2000;
const unsigned long THRESH_REFRESH_MS    = 30000;
const unsigned long MAX_POST_INTERVAL_MS = 300000; // 5 phút keepalive
const unsigned long HTTP_CONN_TIMEOUT_MS = 15000;

// ====== Ngưỡng mặc định ======
float tempDeltaThresh = 1.0f; // °C
float humDeltaThresh  = 1.0f; // %RH

// ====== Trạng thái ======
unsigned long lastSampleMs = 0;
unsigned long lastThreshMs = 0;
unsigned long lastPostMs   = 0;
float lastSentTemp = NAN;
float lastSentHum  = NAN;

WiFiClient netPlain;

// ====== WiFi + NTP ======
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println();
  Serial.printf("WiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());
}

void setupTime() {
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
  Serial.print("Syncing time");
  for (int i = 0; i < 20; i++) {
    time_t now = time(nullptr);
    if (now > 1700000000) break;
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  time_t now = time(nullptr);
  Serial.printf("Time: %s", ctime(&now));
}

// ====== HTTP helpers ======
void prepHTTPClient(HTTPClient& http) {
  http.setConnectTimeout(HTTP_CONN_TIMEOUT_MS);
  http.setTimeout(HTTP_CONN_TIMEOUT_MS);
#if defined(HTTPCLIENT_1_2_COMPAT) || ARDUINO_ESP32_RELEASE >= 0x020000
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS); // tránh 307
#endif
  http.setUserAgent("esp32-dht22-ngrok/1.0");
}

bool httpGET_http(const char* host, uint16_t port, const String& path, String& out) {
  HTTPClient http;
  prepHTTPClient(http);
  if (!http.begin(netPlain, host, port, path)) {
    Serial.println("HTTP begin() failed (GET)");
    return false;
  }
  int code = http.GET();
  out = http.getString();
  http.end();
  Serial.printf("GET http://%s%s => %d\n", host, path.c_str(), code);
  // Serial.println(out);
  return code >= 200 && code < 300;
}

bool httpPOST_http(const char* host, uint16_t port, const String& path, const String& body, String* out = nullptr) {
  HTTPClient http;
  prepHTTPClient(http);
  if (!http.begin(netPlain, host, port, path)) {
    Serial.println("HTTP begin() failed (POST)");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  if (out) *out = resp;
  Serial.printf("POST http://%s%s => %d\n", host, path.c_str(), code);
  Serial.println(resp);
  return code >= 200 && code < 300;
}

// ====== JSON helper đơn giản ======
bool parseJsonFloat(const String& json, const char* key, float& outVal) {
  String k = String("\"") + key + "\"";
  int i = json.indexOf(k); if (i < 0) return false;
  i = json.indexOf(':', i); if (i < 0) return false;
  int j = i + 1; while (j < (int)json.length() && isspace((unsigned char)json[j])) j++;
  bool dot = false, digit = false; int s = j;
  if (j < (int)json.length() && (json[j] == '-' || json[j] == '+')) j++;
  while (j < (int)json.length()) {
    char c = json[j];
    if (c >= '0' && c <= '9') { digit = true; j++; continue; }
    if (c == '.' && !dot) { dot = true; j++; continue; }
    break;
  }
  if (!digit) return false;
  outVal = json.substring(s, j).toFloat();
  return true;
}

// ====== Business logic ======
void refreshThresholds() {
  String path = String(PATH_THRESH) + "?device_id=" + DEVICE_ID;
  String body;
  if (httpGET_http(NGROK_HOST, NGROK_HTTP_PORT, path, body)) {
    float td, hd;
    bool okT = parseJsonFloat(body, "temp_delta", td);
    bool okH = parseJsonFloat(body, "hum_delta",  hd);
    if (okT) tempDeltaThresh = constrain(td, 0.1f, 10.0f);
    if (okH) humDeltaThresh  = constrain(hd, 0.1f, 10.0f);
    Serial.printf("Thresholds => ΔT=%.1f °C, ΔH=%.1f %%\n", tempDeltaThresh, humDeltaThresh);
  } else {
    Serial.println("Refresh thresholds failed (HTTP).");
  }
}

void sendReading(float t, float h) {
  String body;
  body.reserve(160);
  body += "{";
  body += "\"api_key\":\""; body += API_KEY; body += "\",";
  body += "\"device_id\":\""; body += DEVICE_ID; body += "\",";
  body += "\"temperature\":"; body += String(t, 1); body += ",";
  body += "\"humidity\":"; body += String(h, 1);
  body += "}";

  String resp;
  if (httpPOST_http(NGROK_HOST, NGROK_HTTP_PORT, PATH_INGEST, body, &resp)) {
    lastPostMs = millis();
    lastSentTemp = t;
    lastSentHum  = h;
  }
}

// ====== Arduino ======
void setup() {
  Serial.begin(115200);
  delay(200);
  dht.begin();
  connectWiFi();
  setupTime();

  // Health HTTP
  String health;
  if (httpGET_http(NGROK_HOST, NGROK_HTTP_PORT, PATH_HEALTH, health)) {
    Serial.println("Health (HTTP) OK: " + health);
  } else {
    Serial.println("Health (HTTP) FAILED.");
  }

  refreshThresholds();
  lastPostMs = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("DHT read failed");
    } else {
      Serial.printf("DHT => T=%.1f °C, H=%.1f %%\n", t, h);
      bool first = isnan(lastSentTemp) || isnan(lastSentHum);
      bool overT = first ? true : (fabsf(t - lastSentTemp) >= tempDeltaThresh);
      bool overH = first ? true : (fabsf(h - lastSentHum) >= humDeltaThresh);
      bool due   = (now - lastPostMs >= MAX_POST_INTERVAL_MS);
      if (overT || overH || due) {
        Serial.println("Posting (threshold reached or keepalive)...");
        sendReading(t, h);
      }
    }
  }

  if (now - lastThreshMs >= THRESH_REFRESH_MS) {
    lastThreshMs = now;
    refreshThresholds();
  }

  delay(20);
}