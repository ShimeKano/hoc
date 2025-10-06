#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Keypad.h>

// ===== Buzzer =====
constexpr uint8_t BUZZER_PIN = 25;

// ===== WiFi & MQTT =====
const char* ssid         = "Wokwi-GUEST";
const char* password     = "";
const char* mqtt_server  = "broker.emqx.io";
const uint16_t mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// ===== Keypad 4x4 (theo khoacua/diagram.json) =====
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// R1 -> D13, R2 -> D23, R3 -> D19, R4 -> D18
// C1 -> D5,  C2 -> D4,  C3 -> D2,  C4 -> D15
byte rowPins[ROWS] = { 13, 23, 19, 18 };
byte colPins[COLS] = { 5,  4,  2,  15 };

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== Topics =====
const char* TOPIC_TYPED    = "tuanprohl/keypad/typed";
const char* TOPIC_ENTERED  = "tuanprohl/keypad/entered";
const char* TOPIC_RESULT   = "tuanprohl/keypad/result";

// ===== Buffer =====
String inputBuf;
bool waitingResult = false;
unsigned long waitStartMs = 0;
const unsigned long WAIT_RESULT_TIMEOUT_MS = 5000;

// ===== Buzzer (PWM) =====
const int BUZZ_CH = 0;
void buzzerOn(uint16_t freq = 2000) { ledcWriteTone(BUZZ_CH, freq); }
void buzzerOff() { ledcWriteTone(BUZZ_CH, 0); }
void beepOnce(uint16_t freq = 2000, uint16_t onMs = 180, uint16_t offMs = 120) {
  buzzerOn(freq);
  unsigned long t0 = millis();
  while (millis() - t0 < onMs) { client.loop(); delay(1); }
  buzzerOff();
  t0 = millis();
  while (millis() - t0 < offMs) { client.loop(); delay(1); }
}
void beepOK() { beepOnce(2000, 220, 80); }             // 1 hồi
void beepFAIL() { for (int i=0;i<3;i++) beepOnce(1800,150,100); } // 3 hồi

// ===== WiFi/MQTT =====
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Kết nối WiFi: "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\nWiFi OK, IP: " + WiFi.localIP().toString());
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String tpc = String(topic);
  String data; data.reserve(length);
  for (unsigned int i=0;i<length;i++) data += (char)payload[i];
  Serial.print("[MQTT] "); Serial.print(tpc); Serial.print(" => "); Serial.println(data);

  if (tpc == TOPIC_RESULT && waitingResult) {
    String p = data; p.trim();
    if (p.equalsIgnoreCase("OK")) {
      Serial.println("Result: OK (beep 1)");
      beepOK();
    } else {
      Serial.println("Result: FAIL (beep 3)");
      beepFAIL();
    }
    waitingResult = false;
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT connect...");
    String cid = "ESP32-Keypad-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (client.connect(cid.c_str())) {
      Serial.println("OK");
      client.subscribe(TOPIC_RESULT);
      Serial.print("Subscribed: "); Serial.println(TOPIC_RESULT);
    } else {
      Serial.print("fail rc="); Serial.print(client.state()); Serial.println(" retry 2s");
      delay(2000);
    }
  }
}

// ===== Logic =====
void publishTyped() {
  client.publish(TOPIC_TYPED, inputBuf.c_str(), false);
  Serial.print("typed => "); Serial.println(inputBuf);
}
void submitCode() {
  if (inputBuf.length() == 0) return;
  Serial.print("Submit => "); Serial.println(inputBuf);
  client.publish(TOPIC_ENTERED, inputBuf.c_str(), false);
  waitingResult = true;
  waitStartMs = millis();
  inputBuf = "";
  publishTyped();
}
void handleKey(char k) {
  if (k == NO_KEY) return;
  Serial.print("Key: "); Serial.println(k);
  if (k == '#') { submitCode(); return; }
  if (k == '*') { inputBuf = ""; publishTyped(); return; }
  if (k >= '0' && k <= '9') {
    if (inputBuf.length() < 16) { inputBuf += k; publishTyped(); }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  ledcSetup(BUZZ_CH, 2000, 10);
  ledcAttachPin(BUZZER_PIN, BUZZ_CH);
  buzzerOff();

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(onMqttMessage);

  Serial.println("Ready. Nhập số và bấm # để gửi.");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  char k = keypad.getKey();
  handleKey(k);

  if (waitingResult && (millis() - waitStartMs > WAIT_RESULT_TIMEOUT_MS)) {
    Serial.println("Result timeout (web không phản hồi)"); // không beep theo yêu cầu ban đầu
    waitingResult = false;
  }
  delay(5);
}