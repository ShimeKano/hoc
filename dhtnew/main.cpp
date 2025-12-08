#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

#define DHTPIN 4
#define DHTTYPE DHT22

const char* ssid = "Wokwi-GUEST";
const char* pass = "";
const char* mqtt_host = "broker.emqx.io";
const int   mqtt_port = 1883;
const char* topic = "sensors/dht/ShimeKano-esp32-01";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

void reconnect() {
  while (!client.connected()) {
    String cid = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (client.connect(cid.c_str())) {
      Serial.println("MQTT connected");
    } else {
      Serial.printf("MQTT connect failed, rc=%d\n", client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  client.setServer(mqtt_host, mqtt_port);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    char payload[160];
    snprintf(payload, sizeof(payload),
      "{\"device_id\":\"ShimeKano-esp32-01\",\"temperature\":%.2f,\"humidity\":%.2f,\"ts\":%lu}",
      t, h, (unsigned long)(millis()/1000));
    client.publish(topic, payload);
    Serial.println(payload);
  }
  delay(5000);
}