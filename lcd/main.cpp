#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ======== GPIO mapping (theo yêu cầu) ========
constexpr uint8_t PIN_RED    = 19; // Đỏ
constexpr uint8_t PIN_GREEN  = 18; // Xanh
constexpr uint8_t PIN_YELLOW = 5;  // Vàng

// ======== WiFi & MQTT (theo mẫu repo) ========
const char* ssid        = "Wokwi-GUEST";
const char* password    = ""; // Wokwi-GUEST không mật khẩu
const char* mqtt_server = "broker.emqx.io";
const uint16_t mqtt_port = 1883;

// Topics (giữ kiểu "tuanprohl/..." giống repo để đồng bộ)
const char* TOPIC_TL_STATE     = "tuanprohl/traffic/state";      // publish: "GREEN"/"YELLOW"/"RED"
const char* TOPIC_TL_DURATIONS = "tuanprohl/traffic/durations";  // publish: {"green":5,"yellow":2,"red":5}
const char* TOPIC_TL_SET       = "tuanprohl/traffic/set";        // subscribe: {"green":5,"yellow":2,"red":5}
const char* TOPIC_TL_ACK       = "tuanprohl/traffic/ack";        // publish: "OK" hoặc lỗi

// ======== Durations (ms) ========
volatile unsigned long durGreenMs  = 5000;
volatile unsigned long durYellowMs = 2000;
volatile unsigned long durRedMs    = 5000;

// ======== State machine ========
enum class LightState { GREEN, YELLOW, RED };
volatile LightState currentState = LightState::GREEN;
volatile unsigned long lastChange = 0;

WiFiClient espClient;
PubSubClient client(espClient);

// ======== Helpers ========
void applyState(LightState st) {
  switch (st) {
    case LightState::GREEN:
      digitalWrite(PIN_GREEN, HIGH);
      digitalWrite(PIN_YELLOW, LOW);
      digitalWrite(PIN_RED, LOW);
      break;
    case LightState::YELLOW:
      digitalWrite(PIN_GREEN, LOW);
      digitalWrite(PIN_YELLOW, HIGH);
      digitalWrite(PIN_RED, LOW);
      break;
    case LightState::RED:
      digitalWrite(PIN_GREEN, LOW);
      digitalWrite(PIN_YELLOW, LOW);
      digitalWrite(PIN_RED, HIGH);
      break;
  }
}

const char* stateToStr(LightState s) {
  switch (s) {
    case LightState::GREEN:  return "GREEN";
    case LightState::YELLOW: return "YELLOW";
    case LightState::RED:    return "RED";
  }
  return "UNKNOWN";
}

void publishState(LightState s) {
  client.publish(TOPIC_TL_STATE, stateToStr(s), true); // retain để web thấy giá trị cuối cùng
}

void publishDurations() {
  // Publish JSON (đơn vị giây)
  char buf[96];
  snprintf(buf, sizeof(buf),
           "{\"green\":%lu,\"yellow\":%lu,\"red\":%lu}",
           durGreenMs/1000, durYellowMs/1000, durRedMs/1000);
  client.publish(TOPIC_TL_DURATIONS, buf, true);
}

// Parse giá trị integer từ JSON rất đơn giản (không dùng ArduinoJson để nhẹ dependency)
// Tìm khóa "key":<number>
long parseJsonInt(const String& json, const char* key, long fallback) {
  String needle = "\"" + String(key) + "\"";
  int i = json.indexOf(needle);
  if (i < 0) return fallback;
  i = json.indexOf(':', i);
  if (i < 0) return fallback;
  // Bỏ khoảng trắng
  while (i+1 < (int)json.length() && isspace((unsigned char)json[i+1])) i++;
  // Lấy số
  long sign = 1;
  int j = i + 1;
  if (j < (int)json.length() && (json[j] == '-' || json[j] == '+')) {
    if (json[j] == '-') sign = -1;
    j++;
  }
  long val = 0;
  bool hasDigit = false;
  while (j < (int)json.length() && isdigit((unsigned char)json[j])) {
    hasDigit = true;
    val = val * 10 + (json[j] - '0');
    j++;
  }
  if (!hasDigit) return fallback;
  return val * sign;
}

bool clampSeconds(long& s, long minS=1, long maxS=600) {
  if (s < minS) s = minS;
  if (s > maxS) s = maxS;
  return true;
}

void handleSetPayload(const String& payload) {
  // Kỳ vọng JSON: {"green":5,"yellow":2,"red":5} (giây)
  long g = parseJsonInt(payload, "green", -1);
  long y = parseJsonInt(payload, "yellow", -1);
  long r = parseJsonInt(payload, "red", -1);

  bool any = false;
  if (g >= 0) { clampSeconds(g); durGreenMs  = (unsigned long)g * 1000UL; any = true; }
  if (y >= 0) { clampSeconds(y); durYellowMs = (unsigned long)y * 1000UL; any = true; }
  if (r >= 0) { clampSeconds(r); durRedMs    = (unsigned long)r * 1000UL; any = true; }

  if (any) {
    // Áp dụng ngay từ thời điểm hiện tại cho chu kỳ đang chạy
    lastChange = millis();
    publishDurations();
    client.publish(TOPIC_TL_ACK, "OK", false);
  } else {
    client.publish(TOPIC_TL_ACK, "INVALID_PAYLOAD", false);
  }
}

void tickStateMachine() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastChange;

  switch (currentState) {
    case LightState::GREEN:
      if (elapsed >= durGreenMs) {
        currentState = LightState::YELLOW;
        lastChange = now;
        applyState(currentState);
        publishState(currentState);
      }
      break;
    case LightState::YELLOW:
      if (elapsed >= durYellowMs) {
        currentState = LightState::RED;
        lastChange = now;
        applyState(currentState);
        publishState(currentState);
      }
      break;
    case LightState::RED:
      if (elapsed >= durRedMs) {
        currentState = LightState::GREEN;
        lastChange = now;
        applyState(currentState);
        publishState(currentState);
      }
      break;
  }
}

// ======== WiFi/MQTT (theo phong cách repo) ========
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
  Serial.println("\n✅ WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String tpc = String(topic);
  String data; data.reserve(length);
  for (unsigned int i = 0; i < length; i++) data += (char)payload[i];

  if (tpc == TOPIC_TL_SET) {
    handleSetPayload(data);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Đang kết nối MQTT...");
    String clientId = "ESP32-Traffic-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("✅ MQTT connected");
      client.subscribe(TOPIC_TL_SET);
      Serial.print("🔔 Subscribed: "); Serial.println(TOPIC_TL_SET);

      // Gửi trạng thái/durations hiện tại để UI nắm được ngay
      publishState(currentState);
      publishDurations();
    } else {
      Serial.print("❌ failed, rc=");
      Serial.print(client.state());
      Serial.println(" → thử lại sau 5s");
      delay(5000);
    }
  }
}

// ======== Arduino lifecycle ========
void setupPins() {
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  digitalWrite(PIN_RED, LOW);
  digitalWrite(PIN_GREEN, LOW);
  digitalWrite(PIN_YELLOW, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setupPins();

  // Trạng thái ban đầu
  currentState = LightState::GREEN;
  lastChange = millis();
  applyState(currentState);

  // WiFi + MQTT
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  tickStateMachine();
  delay(10);
}