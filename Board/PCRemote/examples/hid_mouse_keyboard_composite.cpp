/
  ESP32-S3 USB HID: Mouse + Keyboard (Composite)
  - GPIO D2 ~ D12를 이용하여 마우스/키보드 제어
  - 시리얼 사용 안 함
  - 마우스 이동/스크롤: 누르고 있는 동안 반복
  - 클릭/키 입력: 에지 트리거(한 번 눌렀을 때 1회 전송)

  권장: Arduino-ESP32 (ESP32-S3) Native USB 사용
/

#ifndef ARDUINOUSBMODE
  #error This ESP32 SoC has no Native USB interface
#elif ARDUINOUSBMODE == 1
  // OTG 모드일 때는 이 스케치를 사용하지 않음
  void setup() {}
  void loop() {}
#else

#include "USB.h"
#include "USBHIDMouse.h"
#include "USBHIDKeyboard.h"

USBHIDMouse Mouse;
USBHIDKeyboard Keyboard;

/ ========================= 사용자 조정 파라미터 ========================= /
// 마우스 이동 1틱 당 픽셀(step). 값이 클수록 빠르게 이동
#define MOUSESTEPPIXELS       8
// 스크롤 1틱 당 양. 양수=위로, 음수=아래로 (OS별 체감 다름)
#define SCROLLSTEPUNITS       1
// 마우스 이동/스크롤 반복 간격 (밀리초). 작을수록 빠르게 반복
#define REPEATINTERVALMS      20
// 디바운스 시간 (밀리초)
#define DEBOUNCEMS             50
/ ====================================================================== /

/ -------------------------- 핀 매핑 --------------------------
   필요에 맞게 바꿔 사용하세요.
/
const int PINMOUSEUP       = D2;
const int PINMOUSEDOWN     = D3;
const int PINMOUSELEFT     = D4;
const int PINMOUSERIGHT    = D5;

const int PINMOUSELCLICK   = D6;
const int PINMOUSERCLICK   = D7;
const int PINMOUSEMCLICK   = D8;

const int PINSCROLLUP      = D9;
const int PINSCROLLDOWN    = D10;

const int PINKEY1          = D11;   // '↑' 키 전송
const int PINKEY2          = D12;   // '↓' 키 전송
/ -------------------------------------------------------------------- /

// 내부 사용: 핀들을 배열로 관리(입력 설정/디바운스 공통 처리)
const int ALLPINS[] = {
  PINMOUSEUP, PINMOUSEDOWN, PINMOUSELEFT, PINMOUSERIGHT,
  PINMOUSELCLICK, PINMOUSERCLICK, PINMOUSEMCLICK,
  PINSCROLLUP, PINSCROLLDOWN, PINKEY1, PINKEY2
};
const sizet NUMPINS = sizeof(ALLPINS) / sizeof(ALLPINS[0]);

// 디바운스/에지 검출 관리용
bool lastStableState[NUMPINS];          // 최근 안정 상태(LOW: 눌림, HIGH: 대기)
bool lastReading[NUMPINS];              // 최근 즉시 읽음
unsigned long lastChangeTime[NUMPINS];  // 마지막 상태 변경 시간
bool pressedEdgeFired[NUMPINS];         // 눌림 에지에서 1회 동작 처리 여부

// 반복 동작(마우스 이동/스크롤) 타이머
unsigned long lastRepeatTime = 0;

void setup() {
  // 입력 풀업 설정
  for (sizet i = 0; i < NUMPINS; i++) {
    pinMode(ALLPINS[i], INPUTPULLUP);
    bool r = digitalRead(ALLPINS[i]);
    lastStableState[i] = r;
    lastReading[i] = r;
    lastChangeTime[i] = millis();
    pressedEdgeFired[i] = false;
  }

  // USB 초기화 (먼저)
  USB.begin();
  // HID 시작
  Mouse.begin();
  Keyboard.begin();
}

/ 유틸: 특정 핀이 현재 눌림 상태(LOW)인지 반환 /
inline bool isPressed(int pin) {
  return digitalRead(pin) == LOW;
}

/ 디바운스: 모든 핀에 대해 안정 상태 갱신 + 에지 플래그 관리 /
void debounceAll() {
  unsigned long now = millis();
  for (sizet i = 0; i < NUMPINS; i++) {
    bool reading = digitalRead(ALLPINS[i]);
    if (reading != lastReading[i]) {
      lastReading[i] = reading;
      lastChangeTime[i] = now;
    }
    // 충분히 유지되면 안정 상태로 채택
    if ((now - lastChangeTime[i]) > DEBOUNCEMS) {
      if (reading != lastStableState[i]) {
        // 상태가 바뀜 → 에지 처리 플래그 초기화
        lastStableState[i] = reading;
        pressedEdgeFired[i] = false; // 새로 눌릴 때 다시 1회 동작 허용
      }
    }
  }
}

/ 에지 트리거(한 번) 동작 수행 도우미 /
void doOnPressOnce(int pin, void (fn)()) {
  // 안정 상태가 LOW(눌림)이고 아직 동작 안 했으면 1회 실행
  // 눌림을 유지하면 추가 실행 없음(떼었다가 다시 눌러야 재실행)
  for (sizet i = 0; i < NUMPINS; i++) {
    if (ALLPINS[i] == pin) {
      if (lastStableState[i] == LOW && !pressedEdgeFired[i]) {
        pressedEdgeFired[i] = true;
        fn();
      }
      break;
    }
  }
}

/ 반복(홀드) 동작 수행 도우미 /
bool isHeld(int pin) {
  for (sizet i = 0; i < NUMPINS; i++) {
    if (ALLPINS[i] == pin) {
      return lastStableState[i] == LOW;
    }
  }
  return false;
}

void loop() {
  debounceAll();
  unsigned long now = millis();

  / ======================== 1) 에지 트리거 동작 =========================
     - 클릭/키 입력은 눌렀을 때 1회만 수행
  /
  // 좌/우/휠 클릭
  doOnPressOnce(PINMOUSELCLICK, []() { Mouse.click(MOUSELEFT);  });
  doOnPressOnce(PINMOUSERCLICK, []() { Mouse.click(MOUSERIGHT); });
  doOnPressOnce(PINMOUSEMCLICK, []() { Mouse.click(MOUSEMIDDLE); });

// 키 입력: ↑ / ↓
  doOnPressOnce(PINKEY1, []() { Keyboard.write(KEYUPARROW); });
  doOnPressOnce(PINKEY2, []() { Keyboard.write(KEYDOWNARROW); });


  / ======================== 2) 홀드 반복 동작 =========================
     - 마우스 이동/스크롤은 누르는 동안 일정 주기로 반복
  /
  if (now - lastRepeatTime >= REPEATINTERVALMS) {
    int dx = 0, dy = 0;
    int8t wheel = 0;  // 수직 스크롤 (USBHIDMouse.move의 3번째 인자)

    if (isHeld(PINMOUSELEFT))  dx -= MOUSESTEPPIXELS;
    if (isHeld(PINMOUSERIGHT)) dx += MOUSESTEPPIXELS;
    if (isHeld(PINMOUSEUP))    dy -= MOUSESTEPPIXELS;
    if (isHeld(PINMOUSEDOWN))  dy += MOUSESTEPPIXELS;

    if (dx != 0 || dy != 0) {
      // move(x, y, wheel=0, hWheel=0)
      Mouse.move(dx, dy, 0, 0);
    }

    if (isHeld(PINSCROLLUP))   wheel += SCROLLSTEPUNITS;
    if (isHeld(PINSCROLLDOWN)) wheel -= SCROLLSTEPUNITS;

    if (wheel != 0) {
      Mouse.move(0, 0, wheel, 0);
    }

    lastRepeatTime = now;
  }

  // 메인 루프 휴식: 너무 작게 잡으면 CPU 점유가 높아질 수 있음
  delay(2);
}

#endif / ARDUINOUSB_MODE */