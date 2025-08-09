#ifndef ARDUINOUSBMODE
  #error This ESP32 SoC has no Native USB interface
#elif ARDUINOUSBMODE == 1
  void setup() {}
  void loop() {}
#else

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"

USBHIDKeyboard Keyboard;
USBHIDMouse    Mouse;

/ ===== 사용자 조정 ===== /
#define INITIALREPEATDELAYMS 300
#define REPEATINTERVALMS        30
#define DEBOUNCEMS               40
#define MOUSEMOVESTEP            8
#define SCROLLSTEP                1
/ ======================= /

const int MODEPIN = D0;

/ ---- 핀 매핑 ---- /
// 마우스
const int PINMOUSEUP     = D2;
const int PINMOUSEDOWN   = D3;
const int PINMOUSELEFT   = D4;
const int PINMOUSERIGHT  = D5;
const int PINMOUSELCLICK = D6;
const int PINMOUSERCLICK = D7;
const int PINMOUSEMCLICK = D8;
const int PINSCROLLUP    = D9;
const int PINSCROLLDOWN  = D10;

// 키보드
const int PINKEYUP       = D11;
const int PINKEYDOWN     = D12;

const int ALLPINS[] = {
  PINMOUSEUP, PINMOUSEDOWN, PINMOUSELEFT, PINMOUSERIGHT,
  PINMOUSELCLICK, PINMOUSERCLICK, PINMOUSEMCLICK,
  PINSCROLLUP, PINSCROLLDOWN,
  PINKEYUP, PINKEYDOWN
};
const sizet NUMPINS = sizeof(ALLPINS) / sizeof(ALLPINS[0]);

bool lastStable[NUMPINS];
bool lastReading[NUMPINS];
unsigned long lastChange[NUMPINS];
bool firedOnce[NUMPINS];
unsigned long firstPressTime[NUMPINS];
unsigned long lastRepeatTime[NUMPINS];

int idxOfPin(int pin){
  for(sizet i=0;i<NUMPINS;i++) if(ALLPINS[i]==pin) return (int)i;
  return -1;
}

void debounceAll(){
  unsigned long now = millis();
  for(sizet i=0;i<NUMPINS;i++){
    bool r = digitalRead(ALLPINS[i]);
    if(r != lastReading[i]){ lastReading[i] = r; lastChange[i] = now; }
    if((now - lastChange[i]) > DEBOUNCEMS){
      if(r != lastStable[i]){
        lastStable[i] = r;
        if(r == LOW){
          firedOnce[i]      = false;
          firstPressTime[i] = now;
          lastRepeatTime[i] = now;
        }
      }
    }
  }
}

bool isHeld(int pin){
  int k = idxOfPin(pin);
  return (k >= 0) && (lastStable[k] == LOW);
}

void pressOnceMouse(int pin, uint8t mouseButton){
  int k = idxOfPin(pin);
  if(k < 0) return;
  if(lastStable[k] == LOW && !firedOnce[k]){
    firedOnce[k] = true;
    Mouse.click(mouseButton);
  }
}

void handleHoldMouseMove(int pin, int dx, int dy){
  int k = idxOfPin(pin);
  if(k < 0) return;
  unsigned long now = millis();
  if(lastStable[k] == LOW){
    if(lastRepeatTime[k] == firstPressTime[k]){
      Mouse.move(dx, dy, 0, 0);
      lastRepeatTime[k] = now;
      return;
    }
    if((now - firstPressTime[k]) >= INITIALREPEATDELAYMS){
      if((now - lastRepeatTime[k]) >= REPEATINTERVALMS){
        Mouse.move(dx, dy, 0, 0);
        lastRepeatTime[k] = now;
      }
    }
  }
}

void handleHoldMouseScroll(int pin, int8t scrollVal){
  int k = idxOfPin(pin);
  if(k < 0) return;
  unsigned long now = millis();
  if(lastStable[k] == LOW){
    if(lastRepeatTime[k] == firstPressTime[k]){
      Mouse.move(0, 0, scrollVal, 0);
      lastRepeatTime[k] = now;
      return;
    }
    if((now - firstPressTime[k]) >= INITIALREPEATDELAYMS){
      if((now - lastRepeatTime[k]) >= REPEATINTERVALMS){
        Mouse.move(0, 0, scrollVal, 0);
        lastRepeatTime[k] = now;
      }
    }
  }
}

void handleHoldKey(int pin, uint8t keycode){
  int k = idxOfPin(pin);
  if(k < 0) return;
  unsigned long now = millis();
  if(lastStable[k] == LOW){
    if(lastRepeatTime[k] == firstPressTime[k]){
      Keyboard.write(keycode);
      lastRepeatTime[k] = now;
      return;
    }
    if((now - firstPressTime[k]) >= INITIALREPEATDELAYMS){
      if((now - lastRepeatTime[k]) >= REPEATINTERVALMS){
        Keyboard.write(keycode);
        lastRepeatTime[k] = now;
      }
    }
  }
}

void setup(){
  pinMode(MODEPIN, INPUTPULLUP);
  for(sizet i=0;i<NUMPINS;i++){
    pinMode(ALLPINS[i], INPUTPULLUP);
    bool r = digitalRead(ALLPINS[i]);
    lastStable[i]  = r;
    lastReading[i] = r;
    lastChange[i]  = millis();
    firedOnce[i]   = false;
    firstPressTime[i] = 0;
    lastRepeatTime[i] = 0;
  }

  bool keyboardonly = (digitalRead(MODEPIN) == LOW);
  Keyboard.begin();
  if(!keyboardonly){
    Mouse.begin();
  }
  USB.begin();
  delay(50);
}

void loop(){
  debounceAll();

  // 마우스 이동 (홀드)
  handleHoldMouseMove(PINMOUSEUP,    0, -MOUSEMOVESTEP);
  handleHoldMouseMove(PINMOUSEDOWN,  0,  MOUSEMOVESTEP);
  handleHoldMouseMove(PINMOUSELEFT, -MOUSEMOVESTEP, 0);
  handleHoldMouseMove(PINMOUSERIGHT,  MOUSEMOVESTEP, 0);

  // 마우스 클릭 (1회)
  pressOnceMouse(PINMOUSELCLICK, MOUSELEFT);
  pressOnceMouse(PINMOUSERCLICK, MOUSERIGHT);
  pressOnceMouse(PINMOUSEMCLICK, MOUSEMIDDLE);

  // 마우스 스크롤 (홀드)
  handleHoldMouseScroll(PINSCROLLUP,   SCROLLSTEP);
  handleHoldMouseScroll(PINSCROLLDOWN, -SCROLLSTEP);

  // 키보드 방향키 (홀드)
  handleHoldKey(PINKEYUP,   KEYUPARROW);
  handleHoldKey(PINKEYDOWN, KEYDOWN_ARROW);

  delay(2);
}

#endif