# dht22_led_VS_Code_Dev_Tunnels

Thư mục này sẽ chứa dự án DHT22 + LED (GPIO17) chạy với PHP API, UI realtime (SSE) và ESP32 (Wokwi/PlatformIO), sử dụng VS Code Dev Tunnels.

Bạn muốn mình đẩy đầy đủ mã nguồn (api/, ui/, schema.sql, firmware/ PlatformIO, wokwi/) vào đây không? Nếu đồng ý, mình sẽ tạo các file và cấu trúc như sau:

- api/ (PHP + MySQL: /readings, /led, /led-state, /history, /sse)
- ui/ (Web dashboard realtime + biểu đồ, điều khiển LED)
- schema.sql (CSDL)
- wokwi/ (sơ đồ mô phỏng)
- firmware/ (PlatformIO cho ESP32)

Trả lời "Đồng ý" để mình push toàn bộ mã nguồn vào thư mục này.