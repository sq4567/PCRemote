#ifndef ARDUINOUSBMODE
  #error This ESP32 SoC has no Native USB interface
#elif ARDUINOUSBMODE == 1
  void setup() {}
  void loop() {}
#else

#include "USB.h"
#include "USBHIDKeyboard.h"

USBHIDKeyboard Keyboard;

/ ===== 사용자 조정 ===== /
#define INITIALREPEATDELAYMS 300  // 홀드 시작 후 반복 시작까지 지연
#define REPEATINTERVALMS       30  // 반복 간격
#define DEBOUNCEMS              40  // 디바운스 시간
/ ======================= /

// 핀 매핑
const int PINUP        = D2;
const int PINDOWN      = D3;
const int PINLEFT      = D4;
const int PINRIGHT     = D5;

const int PINENTER     = D6;
const int PINESC       = D7;
const int PINTAB       = D8;
const int PINBSPC      = D9;
const int PINF2        = D10;
const int PINUPONCE   = D11;   // ↑ 1회 전송
const int PINDOWNONCE = D12;   // ↓ 1회 전송

const int ALLPINS[] = {
  PINUP, PINDOWN, PINLEFT, PINRIGHT,
  PINENTER, PINESC, PINTAB, PINBSPC, PINF2,
  PINUPONCE, PINDOWNONCE
};
const sizet NUMPINS = sizeof(ALLPINS) / sizeof(ALLPINS[0]);

// 디바운스 상태
bool lastStable[NUMPINS];
bool lastReading[NUMPINS];
unsigned long lastChange[NUMPINS];
bool firedOnce[NUMPINS];

// 반복 제어 상태
unsigned long firstPressTime[NUMPINS];  // 처음 눌림 시각
unsigned long lastRepeatTime[NUMPINS];  // 마지막 전송 시각

int idxOfPin(int pin) {
  for (sizet i = 0; i < NUMPINS; i++) if (ALLPINS[i] == pin) return (int)i;
  return -1;
}

void debounceAll() {
  unsigned long now = millis();
  for (sizet i = 0; i < NUMPINS; i++) {
    bool r = digitalRead(ALLPINS[i]);
    if (r != lastReading[i]) {
      lastReading[i] = r;
      lastChange[i] = now;
    }
    if ((now - lastChange[i]) > DEBOUNCEMS) {
      if (r != lastStable[i]) {
        lastStable[i] = r;
        if (r == LOW) {
          firedOnce[i] = false;         // 새 눌림
          firstPressTime[i] = now;      // 첫 눌림 시각 저장
          lastRepeatTime[i] = now;      // 반복 타이머 초기화
        }
      }
    }
  }
}

bool isHeld(int pin) {
  int k = idxOfPin(pin);
  return (k >= 0) ? (lastStable[k] == LOW) : false;
}

void pressOnce(int pin, uint8t keycode) {
  int k = idxOfPin(pin);
  if (k < 0) return;
  if (lastStable[k] == LOW && !firedOnce[k]) {
    firedOnce[k] = true;
    Keyboard.write(keycode);
  }
}

void handleHoldKey(int pin, uint8t keycode) {
  int k = idxOfPin(pin);
  if (k < 0) return;
  unsigned long now = millis();
  if (lastStable[k] == LOW) {
    // 처음 눌렀을 때 즉시 1회 전송
    if (lastRepeatTime[k] == firstPressTime[k]) {
      Keyboard.write(keycode);
      lastRepeatTime[k] = now;
      return;
    }
    // 300ms 지연 이후부터 반복 시작
    if ((now - firstPressTime[k]) >= INITIALREPEATDELAYMS) {
      if ((now - lastRepeatTime[k]) >= REPEATINTERVALMS) {
        Keyboard.write(keycode);
        lastRepeatTime[k] = now;
      }
    }
  }
}

void setup() {
  for (sizet i = 0; i < NUMPINS; i++) {
    pinMode(ALLPINS[i], INPUTPULLUP);
    bool r = digitalRead(ALLPINS[i]);
    lastStable[i] = lastReading[i] = r;
    lastChange[i] = millis();
    firedOnce[i] = false;
    firstPressTime[i] = 0;
    lastRepeatTime[i] = 0;
  }

  USB.begin();
  Keyboard.begin();
  delay(50); // BIOS 초기 인식 대기
}

void loop() {
  debounceAll();

  // 1) 에지 1회 전송 키들
  pressOnce(PINENTER,     KEYRETURN);
  pressOnce(PINESC,       KEYESC);
  pressOnce(PINTAB,       KEYTAB);
  pressOnce(PINBSPC,      KEYBACKSPACE);
  pressOnce(PINF2,        KEYF2);
  pressOnce(PINUPONCE,   KEYUPARROW);
  pressOnce(PINDOWNONCE, KEYDOWNARROW);

  // 2) 홀드 반복 키들 (300ms 지연 후 반복)
  handleHoldKey(PINUP,    KEYUPARROW);
  handleHoldKey(PINDOWN,  KEYDOWNARROW);
  handleHoldKey(PINLEFT,  KEYLEFTARROW);
  handleHoldKey(PINRIGHT, KEYRIGHTARROW);

  delay(2);
}

#endif