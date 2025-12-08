<?php
require __DIR__ . '/config.php';

// GET /api/history.php?limit=200&device=ShimeKano-esp32-01
if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
  json_response(['error' => 'method not allowed'], 405);
}
$limit = max(1, min(2000, intval($_GET['limit'] ?? 200)));
$device = isset($_GET['device']) ? $_GET['device'] : null;

if ($device) {
  $stmt = pdo()->prepare("SELECT device_id, temperature, humidity, ts, created_at
                         FROM readings WHERE device_id = :d ORDER BY id DESC LIMIT :lim");
  $stmt->bindValue(':d', $device);
  $stmt->bindValue(':lim', $limit, PDO::PARAM_INT);
  $stmt->execute();
} else {
  $stmt = pdo()->prepare("SELECT device_id, temperature, humidity, ts, created_at
                         FROM readings ORDER BY id DESC LIMIT :lim");
  $stmt->bindValue(':lim', $limit, PDO::PARAM_INT);
  $stmt->execute();
}

$rows = $stmt->fetchAll();
json_response($rows);