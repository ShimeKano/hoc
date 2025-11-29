#include <Fuzzy.h>

// Khởi tạo đối tượng Fuzzy
Fuzzy *fuzzy = new Fuzzy();

// --- SETUP FUZZY SYSTEM --- //
void setupFuzzy() {

  // 1) Tạo Input: Độ ẩm
  FuzzyInput *moisture = new FuzzyInput(1);

  FuzzySet *low = new FuzzySet(0, 10, 30, 50);     
  FuzzySet *medium = new FuzzySet(30, 40, 60, 70);
  FuzzySet *high = new FuzzySet(50, 70, 90, 100);

  moisture->addFuzzySet(low);
  moisture->addFuzzySet(medium);
  moisture->addFuzzySet(high);
  fuzzy->addFuzzyInput(moisture);

  // 2) Tạo Output: Thời gian tưới
  FuzzyOutput *waterTime = new FuzzyOutput(1);

  FuzzySet *A = new FuzzySet(40, 60, 80, 90);   // Long
  FuzzySet *B = new FuzzySet(10, 20, 40, 50);   // Medium
  FuzzySet *C = new FuzzySet(0, 5, 15, 20);     // Short

  waterTime->addFuzzySet(A);
  waterTime->addFuzzySet(B);
  waterTime->addFuzzySet(C);
  fuzzy->addFuzzyOutput(waterTime);

  // --- RULES --- //

  // Rule 1: IF Low THEN A
  FuzzyRuleAntecedent *ifLow = new FuzzyRuleAntecedent();
  ifLow->joinSingle(low);
  FuzzyRuleConsequent *thenA = new FuzzyRuleConsequent();
  thenA->addOutput(A);
  fuzzy->addFuzzyRule(new FuzzyRule(1, ifLow, thenA));

  // Rule 2: IF Medium THEN B
  FuzzyRuleAntecedent *ifMedium = new FuzzyRuleAntecedent();
  ifMedium->joinSingle(medium);
  FuzzyRuleConsequent *thenB = new FuzzyRuleConsequent();
  thenB->addOutput(B);
  fuzzy->addFuzzyRule(new FuzzyRule(2, ifMedium, thenB));

  // Rule 3: IF High THEN C
  FuzzyRuleAntecedent *ifHigh = new FuzzyRuleAntecedent();
  ifHigh->joinSingle(high);
  FuzzyRuleConsequent *thenC = new FuzzyRuleConsequent();
  thenC->addOutput(C);
  fuzzy->addFuzzyRule(new FuzzyRule(3, ifHigh, thenC));
}


// ------------------ ARDUINO SETUP ------------------- //
int sensorPin = A0;
int relayPin = 8;

float rawToPercent(int raw) {
  // Tùy cảm biến mà đảo chiều
  return (raw / 1023.0) * 100.0;   // 0–100%
}

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  setupFuzzy();
}


// ------------------ ARDUINO LOOP ------------------- //
void loop() {

  int raw = analogRead(sensorPin);
  float moisture = rawToPercent(raw);

  Serial.print("Moisture: ");
  Serial.println(moisture);

  // Gửi giá trị vào fuzzy
  fuzzy->setInput(1, moisture);
  fuzzy->fuzzify();

  float timeToWater = fuzzy->defuzzify(1);

  Serial.print("Water Time (s) = ");
  Serial.println(timeToWater);

  // Nếu cần tưới
  if (timeToWater > 1) {
    digitalWrite(relayPin, HIGH);
    delay((int)(timeToWater * 1000));
    digitalWrite(relayPin, LOW);
  }

  delay(5000); // 5 giây đọc lại
}
