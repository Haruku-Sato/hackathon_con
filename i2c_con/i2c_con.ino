// 指揮者側 A0 検知のみ（送信・BPM処理なし）
const int SENSOR_PIN = A0;

unsigned long t1 = 0, t2 = 0;
int detectCount = 0;
bool isSensorPressed = false;

// チャタリング判定閾値[ms]
const unsigned long CHATTER_THRESHOLD = 200;

// t2 検出後の検知停止時間[ms]（この間は t1 を取り直さない）
const unsigned long COOLDOWN_MS = 500;
unsigned long cooldownUntil = 0;

unsigned long detectPress(int sensorPin) {
  int sensorState = analogRead(sensorPin);
  if (sensorState > 150 && !isSensorPressed) {
    isSensorPressed = true;
    return millis();
  } else if (sensorState <= 150 && isSensorPressed) {
    isSensorPressed = false;
  }
  return 0;
}

void setup() {
  Serial.begin(9600);
  pinMode(SENSOR_PIN, INPUT);
  Serial.println("=== A0 検知のみモード ===");
}

void loop() {
  // クールダウン中は検知を停止（センサの残り押下状態だけは更新しておく）
  if (millis() < cooldownUntil) {
    int s = analogRead(SENSOR_PIN);
    if (s <= 150) isSensorPressed = false;
    return;
  }

  unsigned long now = detectPress(SENSOR_PIN);
  if (now == 0) return;

  detectCount++;
  if (detectCount == 1) {
    t1 = now;
    Serial.print("【1回目】 t1 = "); Serial.println(t1);
  } else if (detectCount == 2) {
    if (now - t1 < CHATTER_THRESHOLD) {
      Serial.print("【チャタリング検出】無視: 差=");
      Serial.println(now - t1);
      detectCount = 1;  // t1 のまま次を待つ
      return;
    }
    t2 = now;
    Serial.print("【2回目】 t2 = "); Serial.println(t2);
    Serial.print("差(t2 - t1) = "); Serial.println(t2 - t1);

    // 次サイクルへ。クールダウン中は検知停止
    detectCount = 0;
    cooldownUntil = millis() + COOLDOWN_MS;
  }
}
