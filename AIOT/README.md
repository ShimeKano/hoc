---
title: Smart Home AI Command Center
emoji: 🛡️
colorFrom: blue
colorTo: indigo
sdk: gradio
sdk_version: 6.2.0
app_file: app.py
pinned: false
license: apache-2.0
---

# 🏠 Smart Home AI Dashboard
Hệ thống điều khiển nhà thông minh tích hợp Face ID và Giám sát Gas AI.

## 🚀 Tính năng
* Face ID: Nhận diện chủ nhà Quốc Tuấn và gửi lệnh OPEN/LOCK qua MQTT.
* AI Gas Monitor: Phát hiện bất thường bằng Isolation Forest.
* Real-time Chart: Biểu đồ nồng độ khí Gas cập nhật mỗi 3 giây.

## Cấu trúc mới
```
AIOT/
  app.py            # điểm vào chạy app
  config.py         # hằng số cấu hình
  ui.py             # giao diện Gradio + glue
  ai/
    face_id.py      # Facenet + nhận diện
    gas.py          # IsolationForest + buffer biểu đồ
  mqtt_client.py    # MQTT connector + publish
```