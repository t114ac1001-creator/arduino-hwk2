/*
  HW2: Interactive Toy
  - RGB LED 顯示玩偶心情
      綠色 = 開心 (happy)   → 呼吸燈
      黃色 = 普通 (neutral)
      藍色 = 傷心 (sad)     → 緩慢閃爍
      紅色 = 生氣 (angry)   → 快速閃爍（被連續亂按時觸發）
  - 按壓玩偶 → 心情分數增加
  - 太久沒人按 → 心情分數下降
  - 額外功能(加分)：連按太快會生氣、Serial Monitor 顯示心情分數
*/

// ===================== 腳位設定 =====================
const int PIN_R = 9;       // RGB LED 紅 (PWM)
const int PIN_G = 10;      // RGB LED 綠 (PWM)
const int PIN_B = 11;      // RGB LED 藍 (PWM)
const int PIN_TOUCH = 2;   // 按鈕模組 OUT 腳

// ===================== 參數設定（可自行調整） =====================
#define COMMON_ANODE true           // 你的模組共用腳是 V（接 5V）→ 共陽極，設為 true
#define BUTTON_ACTIVE_HIGH true     // 按下時 OUT 輸出高電位 → true；若按下輸出低電位 → false
const bool PRESSED_LEVEL = BUTTON_ACTIVE_HIGH ? HIGH : LOW;
const int  MOOD_MAX        = 100;   // 心情分數上限
const int  MOOD_MIN        = 0;     // 心情分數下限
const int  PET_GAIN        = 10;    // 每按一次增加的分數
const unsigned long IDLE_MS  = 5000;  // 多久沒按算「太久沒碰」(毫秒)
const unsigned long DECAY_MS = 1000;  // 沒碰時，每隔多久扣一次分
const int  DECAY_AMOUNT    = 5;     // 每次扣的分數
const int  HAPPY_LEVEL     = 60;    // 分數 >= 此值 → 開心
const int  SAD_LEVEL       = 30;    // 分數 <  此值 → 傷心
const int  SPAM_COUNT      = 6;     // 在時間窗內按幾次算「亂按」
const unsigned long SPAM_WINDOW_MS = 2000;  // 亂按判斷時間窗
const unsigned long ANGRY_MS       = 3000;  // 生氣持續時間
const unsigned long DEBOUNCE_MS    = 40;    // 按鈕防彈跳

// ===================== 變數 =====================
enum Mood { SAD, NEUTRAL, HAPPY, ANGRY };

int mood = 50;                         // 目前心情分數
unsigned long lastTouchTime = 0;       // 上次被按的時間
unsigned long lastDecayTime = 0;       // 上次扣分的時間
unsigned long angryUntil = 0;          // 生氣結束時間

int spamCount = 0;                     // 時間窗內按壓次數
unsigned long spamWindowStart = 0;

bool lastReading = !PRESSED_LEVEL;     // 按鈕防彈跳用（初始為未按下）
bool stableState = !PRESSED_LEVEL;
unsigned long lastDebounceTime = 0;

// ===================== LED 輸出 =====================
void setColor(int r, int g, int b) {
  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);
  if (COMMON_ANODE) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  analogWrite(PIN_R, r);
  analogWrite(PIN_G, g);
  analogWrite(PIN_B, b);
}

// ===================== 按壓偵測（回傳 true 代表剛被按下一次） =====================
bool pressedEvent() {
  bool reading = digitalRead(PIN_TOUCH);
  bool event = false;

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }
  if (millis() - lastDebounceTime > DEBOUNCE_MS) {
    if (reading != stableState) {
      stableState = reading;
      if (stableState == PRESSED_LEVEL) {   // 按下
        event = true;
      }
    }
  }
  lastReading = reading;
  return event;
}

// ===================== 取得目前心情 =====================
Mood currentMood(unsigned long now) {
  if (now < angryUntil)      return ANGRY;
  if (mood >= HAPPY_LEVEL)   return HAPPY;
  if (mood < SAD_LEVEL)      return SAD;
  return NEUTRAL;
}

// ===================== 更新 LED =====================
void updateLED(unsigned long now) {
  switch (currentMood(now)) {
    case HAPPY: {                       // 綠色呼吸燈
      float k = 0.65 + 0.35 * sin(now / 250.0);
      setColor(0, 255 * k, 0);
      break;
    }
    case NEUTRAL:                       // 黃色
      setColor(255, 160, 0);
      break;
    case SAD: {                         // 藍色慢閃
      float k = 0.35 + 0.65 * (0.5 + 0.5 * sin(now / 600.0));
      setColor(0, 0, 255 * k);
      break;
    }
    case ANGRY:                         // 紅色快閃
      if ((now / 150) % 2 == 0) setColor(255, 0, 0);
      else                      setColor(60, 0, 0);
      break;
  }
}

// ===================== 初始化 =====================
void setup() {
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_TOUCH, INPUT);   // 模組板上已有電阻，不需內部上拉
  Serial.begin(9600);

  lastTouchTime = millis();
  lastDecayTime = millis();
  spamWindowStart = millis();

  // 開機燈光測試：紅 → 綠 → 藍
  setColor(255, 0, 0); delay(300);
  setColor(0, 255, 0); delay(300);
  setColor(0, 0, 255); delay(300);
  setColor(0, 0, 0);

  Serial.println("Interactive Toy ready!");
}

// ===================== 主迴圈 =====================
void loop() {
  unsigned long now = millis();

  // 1) 偵測按壓
  if (pressedEvent()) {
    lastTouchTime = now;
    lastDecayTime = now;

    // 亂按偵測
    if (now - spamWindowStart > SPAM_WINDOW_MS) {
      spamWindowStart = now;
      spamCount = 0;
    }
    spamCount++;

    if (spamCount >= SPAM_COUNT) {
      // 被連續亂按 → 生氣、扣分
      angryUntil = now + ANGRY_MS;
      mood = max(MOOD_MIN, mood - 10);
      spamCount = 0;
      Serial.println("Too many pets! Angry!");
    } else if (now >= angryUntil) {
      // 正常撫摸 → 加分
      mood = min(MOOD_MAX, mood + PET_GAIN);
    }
    Serial.print("Mood points: ");
    Serial.println(mood);
  }

  // 2) 太久沒碰 → 扣分
  if (now - lastTouchTime > IDLE_MS && now - lastDecayTime >= DECAY_MS) {
    lastDecayTime = now;
    if (mood > MOOD_MIN) {
      mood = max(MOOD_MIN, mood - DECAY_AMOUNT);
      Serial.print("Lonely... Mood points: ");
      Serial.println(mood);
    }
  }

  // 3) 更新 LED
  updateLED(now);
}
