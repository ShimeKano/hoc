# Global configuration and constants for your Space

import os

# The Space itself holds the template file, so use repo_type="space"
HF_REPO_ID   = os.getenv("HF_REPO_ID", "balenkano/nhandienkhuonmat")
HF_REPO_TYPE = os.getenv("HF_REPO_TYPE", "space")
# Đặt đúng đường dẫn file trong repo Space (ví dụ để ở root)
HF_FILENAME  = os.getenv("HF_FILENAME",  "quoc_tuan_template.npz")

# MQTT
MQTT_BROKER = os.getenv("MQTT_BROKER", "test.mosquitto.org")
MQTT_PORT   = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC_GAS_IN   = os.getenv("MQTT_TOPIC_GAS_IN", "balenkano_tuan_gas_data")
MQTT_TOPIC_DOOR_CMD = os.getenv("MQTT_TOPIC_DOOR_CMD", "balenkano_tuan_door_2025")

# UI
UI_UPDATE_INTERVAL_SECONDS = int(os.getenv("UI_UPDATE_INTERVAL_SECONDS", "3"))
GAS_CHART_MAX_POINTS = int(os.getenv("GAS_CHART_MAX_POINTS", "15"))
GAS_CHART_Y_LIM = [0, 4095]