#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin cấu hình
#define LED1_PIN 4
#define LED2_PIN 16
#define BUTTON_PIN 17

// LCD I2C: Địa chỉ Wokwi mặc định 0x27, 16x2
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Trạng thái toàn cục
volatile bool led1_state = false;
volatile bool led2_state = false;
volatile bool toggle_mode = false; // Chế độ xen kẽ

// FreeRTOS Task Handles
TaskHandle_t TaskLED1Handle = NULL;
TaskHandle_t TaskLED2Handle = NULL;
TaskHandle_t TaskLCDHandle = NULL;
TaskHandle_t TaskButtonHandle = NULL;

// Task LED1: Bật 2s, tắt 1s (chu kỳ 3s) hoặc xen kẽ
void TaskLED1(void *pvParameters) {
  while (true) {
    if (toggle_mode) {
      // Chế độ xen kẽ: LED1 bật, LED2 tắt 0.5s
      led1_state = true;
      led2_state = false;
      digitalWrite(LED1_PIN, HIGH);
      digitalWrite(LED2_PIN, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);

      // LED1 tắt, LED2 bật 0.5s
      led1_state = false;
      led2_state = true;
      digitalWrite(LED1_PIN, LOW);
      digitalWrite(LED2_PIN, HIGH);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    } else {
      // Bình thường: LED1 bật 2s, tắt 1s
      led1_state = true;
      digitalWrite(LED1_PIN, HIGH);
      vTaskDelay(2000 / portTICK_PERIOD_MS);

      led1_state = false;
      digitalWrite(LED1_PIN, LOW);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

// Task LED2: Bật 2.5s, tắt 0.5s (chu kỳ 3s) hoặc xen kẽ
void TaskLED2(void *pvParameters) {
  while (true) {
    if (!toggle_mode) {
      led2_state = true;
      digitalWrite(LED2_PIN, HIGH);
      vTaskDelay(2500 / portTICK_PERIOD_MS);

      led2_state = false;
      digitalWrite(LED2_PIN, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    } else {
      // Chế độ xen kẽ do TaskLED1 điều khiển, chỉ sleep ngắn
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
}

// Task LCD: Hiển thị trạng thái LED liên tục
void TaskLCD(void *pvParameters) {
  while (true) {
    lcd.setCursor(0, 0);
    lcd.print("LED1: ");
    lcd.print(led1_state ? "ON " : "OFF");
    lcd.setCursor(0, 1);
    lcd.print("LED2: ");
    lcd.print(led2_state ? "ON " : "OFF");
    vTaskDelay(200 / portTICK_PERIOD_MS); // 5 lần mỗi giây
  }
}

// Task Button: Đọc trạng thái nút nhấn D (chân 17), chống dội phím
void TaskButton(void *pvParameters) {
  bool last_state = HIGH;
  while (true) {
    bool current_state = digitalRead(BUTTON_PIN);
    if (last_state == HIGH && current_state == LOW) { // Nhấn xuống
      toggle_mode = !toggle_mode; // Đổi chế độ
      // Khi chuyển chế độ, tắt cả 2 LED
      digitalWrite(LED1_PIN, LOW);
      digitalWrite(LED2_PIN, LOW);
      led1_state = false;
      led2_state = false;
      vTaskDelay(200 / portTICK_PERIOD_MS); // Chống dội
    }
    last_state = current_state;
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void setup() {
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  lcd.init();
  lcd.backlight();

  xTaskCreate(TaskLED1, "TaskLED1", 1024, NULL, 1, &TaskLED1Handle);
  xTaskCreate(TaskLED2, "TaskLED2", 1024, NULL, 1, &TaskLED2Handle);
  xTaskCreate(TaskLCD,  "TaskLCD",  2048, NULL, 1, &TaskLCDHandle);  // <--- tăng stack
  xTaskCreate(TaskButton, "TaskButton", 1024, NULL, 2, &TaskButtonHandle);
}

void loop() {
  // Không cần code ở đây, mọi thứ chạy bằng FreeRTOS tasks
}