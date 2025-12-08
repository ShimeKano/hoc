<?php
require __DIR__ . '/config.php';

// POST /api/readings.php  body: {temperature, humidity, ts?}
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
  json_response(['error' => 'method not allowed'], 405);
}
$device_id = auth_device();
if (!$device_id) {
  json_response(['error' => 'unauthorized'], 401);
}

$raw = file_get_contents('php://input');
$data = json_decode($raw, true);
if (!$data || !isset($data['temperature']) || !isset($data['humidity'])) {
  json_response(['error' => 'invalid payload'], 400);
}

$t = (float)$data['temperature'];
$h = (float)$data['humidity'];
$ts = isset($data['ts']) ? (int)$data['ts'] : time();

$stmt = pdo()->prepare('INSERT INTO readings (device_id, temperature, humidity, ts) VALUES (:d,:t,:h,:ts)');
$stmt->execute([':d' => $device_id, ':t' => $t, ':h' => $h, ':ts' => $ts]);

json_response(['ok' => true, 'device_id' => $device_id]);