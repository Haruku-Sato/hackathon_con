//指揮者側I2C
#include <Wire.h>

const int NUM_INSTRUMENTS = 4;
const uint8_t addrInstrument[4] = {0x10, 0x20, 0x30, 0x40};

const int SENSOR_PIN = A0;
unsigned long t1 = 0, t2 = 0;
int detectCount = 0;
bool isSensorPressed = false;
int currentBPM = 120;
int currentBarCount = 0;

bool isSystemPlaying = false;
unsigned long playStartTime = 0;
unsigned long nextBarTime = 0;   // ★次の小節送信時刻

// チャタリング判定閾値[ms]（BPM対応表の最小値515msより小さく設定）
const unsigned long CHATTER_THRESHOLD = 200;

// t2 検出後の検知停止時間[ms]（この間は t1 を取り直さない）
const unsigned long COOLDOWN_MS = 500;
unsigned long cooldownUntil = 0;

// 荷重センサが反応した瞬間の時刻を返す（チャタリング処理は外で行う）
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

unsigned long measureTmeas(int sensorPin) {
  // クールダウン中は検知を停止（センサが離れた状態だけは追従しておく）
  if (millis() < cooldownUntil) {
    int s = analogRead(sensorPin);
    if (s <= 150) isSensorPressed = false;
    return 0;
  }

  unsigned long now = detectPress(sensorPin);
  if (now == 0) return 0;  // 検知なし

  detectCount++;
  if (detectCount == 1) {
    t1 = now;
    Serial.print("【1回目】 t1 = "); Serial.println(t1);
  } else if (detectCount == 2) {
    // チャタリング判定：t1からの差が閾値未満ならt2として認めず破棄
    if (now - t1 < CHATTER_THRESHOLD) {
      Serial.print("【チャタリング検出】無視: 差=");
      Serial.println(now - t1);
      detectCount = 1;  // t2を破棄してt1のまま次を待つ
      return 0;
    }
    t2 = now;
    Serial.print("【2回目】 t2 = "); Serial.println(t2);
    detectCount = 0;
    cooldownUntil = millis() + COOLDOWN_MS;  // 次サイクルまで検知停止
    return t2 - t1;
  }
  return 0;
}

int lookupBPM(unsigned long Tmeas) {
  const int EDGE_COUNT = 3;
  unsigned long thresholds[EDGE_COUNT] = {322, 764, 1150};
  int bpmTable[EDGE_COUNT + 1] = {240, 160, 100, 40};
  for (int i = 0; i < EDGE_COUNT; i++) {
    if (Tmeas < thresholds[i]) return bpmTable[i];
  }
  return bpmTable[EDGE_COUNT];
}
//int lookupBPM(unsigned long Tmeas) {
  //const int EDGE_COUNT = 8;
  //unsigned long thresholds[EDGE_COUNT] = {267, 352, 437, 522, 607, 692, 777, 862};
  //int bpmTable[EDGE_COUNT + 1] = {240, 215, 190, 165, 140, 115, 90, 65, 40};
  //for (int i = 0; i < EDGE_COUNT; i++) {
    //if (Tmeas < thresholds[i]) return bpmTable[i];
  //}
  //return bpmTable[EDGE_COUNT];
//}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  pinMode(SENSOR_PIN, INPUT);
  Serial.println("=== 輪唱システム 指揮者(I2C・荷重センサ版)===");
  Serial.println("車両がセンサを通過するのを待機中...");
}

void loop() {
  // センサ計測は常時実行
  unsigned long tmeas = measureTmeas(SENSOR_PIN);

  if (!isSystemPlaying) {
    // 演奏前:車両通過でBPM決定して演奏開始
    if (tmeas > 0) {
      int bpm = lookupBPM(tmeas);
      Serial.print("Tmeas="); Serial.print(tmeas);
      Serial.print(" → 初期BPM="); Serial.println(bpm);
      currentBPM = bpm;
      isSystemPlaying = true;
      playStartTime = millis();
      currentBarCount = 0;
      nextBarTime = millis();  // 即座に1小節目送信
    }
  } else {
    // 演奏中:BPM更新を検知
    if (tmeas > 0) {
      currentBPM = lookupBPM(tmeas);
      Serial.print("★車両通過!BPMを更新しました -> ");
      Serial.println(currentBPM);
    }

    // 小節タイミングが来たら送信
    if (millis() >= nextBarTime) {
      currentBarCount++;
      Serial.print("【リアルタイム更新】小節: ");
      Serial.print(currentBarCount);
      Serial.print(" / 送信BPM: ");
      Serial.println(currentBPM);

      uint8_t packet[3];
      packet[1] = (uint8_t)currentBPM;

      for (int partID = 0; partID < NUM_INSTRUMENTS; partID++) {
        int startBar = (partID * 2) + 1;
        if (currentBarCount >= startBar) {
          packet[0] = (currentBarCount == startBar) ? 0xAA : 0xBB;
          packet[2] = packet[0] ^ packet[1];
          Wire.beginTransmission(addrInstrument[partID]);
          Wire.write(packet, 3);
          Wire.endTransmission();
        }
      }

      // 次の送信時刻を計算(最新BPMで)
      unsigned long bar_ms = (unsigned long)((60.0 / currentBPM) * 4.0 * 1000.0 );
      nextBarTime = millis() + bar_ms;
    }

    // タイムアウト
    if (millis() - playStartTime > 120000UL) {
      isSystemPlaying = false;
      currentBarCount = 0;
      Serial.println("演奏終了・再受付");
      Serial.println("次の車両を待機中...");
    }
  }
}
