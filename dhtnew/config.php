<?php
// Cấu hình MySQL (sửa theo XAMPP/PhpMyAdmin của bạn)
define('DB_HOST', 'localhost');
define('DB_USER', 'root');
define('DB_PASS', '');      // mặc định XAMPP là rỗng
define('DB_NAME', 'dht22_led'); // database bạn đã tạo

// Nếu muốn bỏ xác thực API, đặt DEVICE_API_KEY = '' (không khuyến nghị)
define('DEVICE_API_KEY_FALLBACK', 'aa9d88e96ffeaff604c3acff96bca3fcd0fe7f8e');

function pdo(): PDO {
  static $pdo = null;
  if ($pdo === null) {
    $dsn = 'mysql:host=' . DB_HOST . ';dbname=' . DB_NAME . ';charset=utf8mb4';
    $pdo = new PDO($dsn, DB_USER, DB_PASS, [
      PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
      PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
    ]);
  }
  return $pdo;
}

// Xác thực API key header X-API-KEY -> trả về device_id hoặc null
function auth_device(): ?string {
  $headers = function_exists('getallheaders') ? getallheaders() : [];
  $api = $headers['X-API-KEY'] ?? $headers['x-api-key'] ?? '';
  if (!$api) return null;
  // kiểm tra bảng devices
  $stmt = pdo()->prepare('SELECT device_id FROM devices WHERE api_key = :k LIMIT 1');
  $stmt->execute([':k' => $api]);
  $row = $stmt->fetch();
  if ($row) return $row['device_id'];
  // fallback: nếu không có devices table / record, chấp nhận DEVICE_API_KEY_FALLBACK
  if ($api === DEVICE_API_KEY_FALLBACK) return 'ShimeKano-esp32-01';
  return null;
}

function json_response($data, int $code = 200) {
  http_response_code($code);
  header('Content-Type: application/json; charset=utf-8');
  echo json_encode($data, JSON_UNESCAPED_UNICODE);
  exit;
}