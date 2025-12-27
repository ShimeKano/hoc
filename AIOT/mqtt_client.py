import threading
import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion

from config import MQTT_BROKER, MQTT_PORT, MQTT_TOPIC_GAS_IN, MQTT_TOPIC_DOOR_CMD

class MQTTService:
    def __init__(self, on_gas_message):
        self.on_gas_message = on_gas_message
        self.client = mqtt.Client(CallbackAPIVersion.VERSION2)
        self.client.on_message = self._on_message

    def _on_message(self, client, userdata, message):
        try:
            val = float(message.payload.decode())
            if self.on_gas_message:
                self.on_gas_message(val)
        except Exception:
            pass

    def start(self):
        try:
            self.client.connect(MQTT_BROKER, MQTT_PORT)
            self.client.subscribe(MQTT_TOPIC_GAS_IN)
            self.client.loop_start()
        except Exception:
            pass  # Spaces có thể chặn mạng; UI vẫn hoạt động

    def publish_cmd(self, cmd: str):
        def _publish():
            try:
                c = mqtt.Client(CallbackAPIVersion.VERSION2)
                c.connect(MQTT_BROKER, MQTT_PORT)
                c.publish(MQTT_TOPIC_DOOR_CMD, cmd)
                c.disconnect()
            except Exception:
                pass
        threading.Thread(target=_publish, daemon=True).start()