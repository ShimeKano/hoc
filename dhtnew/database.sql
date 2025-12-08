-- Tạo database (đổi tên nếu muốn)
CREATE DATABASE IF NOT EXISTS `dht22_led`
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

-- Chọn database
USE `dht22_led`;

-- Bảng devices: đăng ký thiết bị và API key
CREATE TABLE IF NOT EXISTS devices (
  id INT AUTO_INCREMENT PRIMARY KEY,
  device_id VARCHAR(64) NOT NULL UNIQUE,
  api_key CHAR(40) NOT NULL UNIQUE,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Bảng readings: lưu nhiệt độ/độ ẩm từ DHT22
CREATE TABLE IF NOT EXISTS readings (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  device_id VARCHAR(64) NOT NULL,
  temperature DOUBLE NOT NULL,
  humidity DOUBLE NOT NULL,
  ts BIGINT NOT NULL, -- UNIX seconds
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_readings_device_ts (device_id, ts),
  CONSTRAINT fk_readings_device FOREIGN KEY (device_id) REFERENCES devices(device_id)
    ON UPDATE CASCADE ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- (Tùy chọn) Nếu chỉ cần hiển thị dữ liệu, bạn có thể bỏ hai bảng bên dưới.
-- Bảng led_state (nếu sau này muốn điều khiển LED)
CREATE TABLE IF NOT EXISTS led_state (
  id INT AUTO_INCREMENT PRIMARY KEY,
  device_id VARCHAR(64) NOT NULL UNIQUE,
  state TINYINT NOT NULL DEFAULT 0,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CONSTRAINT fk_led_state_device FOREIGN KEY (device_id) REFERENCES devices(device_id)
    ON UPDATE CASCADE ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Bảng commands (hàng đợi lệnh, không bắt buộc cho yêu cầu hiện tại)
CREATE TABLE IF NOT EXISTS commands (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  device_id VARCHAR(64) NOT NULL,
  command_type VARCHAR(32) NOT NULL,
  payload JSON NOT NULL,
  processed TINYINT(1) NOT NULL DEFAULT 0,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_commands_device_processed (device_id, processed),
  CONSTRAINT fk_commands_device FOREIGN KEY (device_id) REFERENCES devices(device_id)
    ON UPDATE CASCADE ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Seed một thiết bị mẫu để firmware ESP32 có thể gửi dữ liệu ngay
INSERT INTO devices (device_id, api_key)
VALUES ('ShimeKano-esp32-01', 'aa9d88e96ffeaff604c3acff96bca3fcd0fe7f8e')
ON DUPLICATE KEY UPDATE api_key = VALUES(api_key);