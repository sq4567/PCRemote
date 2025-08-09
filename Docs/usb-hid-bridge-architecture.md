---
title: "USB HID 브리지 아키텍처 및 구현 가이드"
description: "OTG Y 케이블 기반 Android ↔ USB-Serial ↔ ESP32-S3 ↔ PC(HID) 유선 경로 최종 구성"
tags: ["esp32-s3", "tinyusb", "usb", "hid", "otg-y-cable", "charge-while-otg"]
version: "v1.0"
owner: "Firmware"
updated: "2025-01-13"
status: "confirmed"
---

# USB HID 브리지 아키텍처 및 구현 가이드

## 목차
- 1. 준비물
- 2. 시스템 아키텍처
  - 2.1 엔드투엔드 경로/신뢰성 가정
  - 2.2 상태머신/핸드셰이크
  - 2.3 성능/지연 측정 방법
  - 2.4 충전 병행 구성(OTG Y‑케이블/PD 허브)
- 3. 프로토콜
  - 3.1 seq/유실 판정 및 재동기화
  - 3.2 flags 비트 정의
- 4. 하드웨어 연결
  - 4.1 스마트폰 측 연결
  - 4.2 ESP32-S3 측 연결 (UART 배선)
  - 4.3 전체 연결 다이어그램
- 5. ESP32‑S3 펌웨어(핵심 개념)
  - 5.1 HID Report Descriptor(부트 마우스)
    - 5.1.1 Boot ↔ Report 프로토콜 전환
  - 5.2 전송 루프 설계
    - 5.2.1 재동기화 및 분할 주입
- 6. ESP32‑S3 구현 가이드(ESP‑IDF/TinyUSB, 공식 문서 준수)
  - 6.1 핵심 공식 API(이름 기준)
  - 6.2 설계-구현 매핑 요약
  - 6.3 상태/이벤트 처리 표준
  - 6.4 참조(공식 API 이름)
  - 6.5 Geekble nano ESP32‑S3 핀맵/전기 설정(레퍼런스)
  - 6.6 ESP‑IDF 프로젝트 구조(예시)
  - 6.7 초기화 시퀀스(의사코드)
  - 6.8 UART 8B 프레이밍/재동기화(의사코드)
  - 6.9 델타 분할 주입 규칙(의사코드)
  - 6.10 SET_PROTOCOL 처리(부트/리포트 전환)
  - 6.11 빌드/플래시 절차(ESP‑IDF)
  - 6.12 성능/로깅 파라미터(권장)
- 7. Android(USB‑Serial) 송신 가이드
- 8. 하드웨어 연결 검증 가이드
  - 8.1 단계별 연결 확인
  - 8.2 Android 앱에서 확인하는 방법
  - 8.3 문제 발생 시 단계별 진단
  - 8.4 성공 지표
- 9. 빠른 시작
- 10. 테스트 체크리스트
- 11. 트러블슈팅/가드레일

## 용어집/정의

- BootSafe/Normal: 호스트 OS 단계 진입 전/후 운용 상태. 본 문서의 기본 전제는 BootSafe(부트 마우스)입니다.
- Selected/Unselected: 선택 상태. 본 문서에서는 UI 시각보다 프로토콜에 초점, 필요 시 참조 문서로 위임.
- Enabled/Disabled: 입력 가능 상태. 앱/UI 정책 참조.
- TransportState: NoTransport | UsbOpening | UsbReady.
- 상태 용어 사용 원칙(금칙어 포함): "활성/비활성" 대신 Selected/Unselected, Enabled/Disabled 사용 [[memory:5809234]].

---

## 1. 준비물

- **OTG Y 케이블**
  - **유형**: USB-C OTG Y 케이블 (Charge-while-OTG 지원)
  - **포트 구성**: 
    - USB-C 플러그: 스마트폰 연결용
    - USB-A 포트 (Device/OTG): USB-Serial 어댑터 연결용  
    - USB-C 포트 (Power/PD): 충전기 연결용
  - **핵심 기능: OTG + 동시 충전**
    - 스마트폰이 USB Host 역할을 수행하면서 동시에 충전을 가능하게 하는 필수 케이블입니다.
  - **프로젝트 내 역할**
    - 장시간 사용 시 배터리 소모를 방지하며 안정적인 연결을 보장합니다.
    - 하나의 케이블로 데이터 통신과 전원 공급을 동시에 해결하는 핵심 부품입니다.

- **USB‑Serial 어댑터 (3.3V TTL)**
  - **칩셋**: `CP2102`
  - **사양**: 3.3V 로직(TX/RX/GND), `VCC` 미사용
  - **VID/PID**: CP2102(`0x10C4`/`0xEA60`)
  - **핵심 기능: 신호 변환기 (번역가)**
    - 스마트폰이 사용하는 **USB 통신 방식**과 ESP32-S3 보드가 사용하는 **UART(시리얼) 통신 방식**을 변환합니다.
  - **프로젝트 내 역할**
    - OTG Y 케이블의 Device 포트에 연결되어 Android 앱의 마우스 데이터를 ESP32-S3로 전달합니다.

- **점퍼 케이블** (암‑암 Dupont 3가닥)
  - **핵심 기능: 데이터 통로 (핏줄)**
    - 전자 부품 간 신호를 주고받는 물리적인 연결선입니다.
  - **프로젝트 내 역할**
    - `USB-Serial 어댑터`와 `ESP32-S3 보드` 사이의 물리적인 데이터 통로를 만듭니다. 3가닥의 선은 각각 다음과 같은 역할을 합니다.
      - `TX` (송신): 데이터를 보내는 선
      - `RX` (수신): 데이터를 받는 선
      - `GND` (접지): 안정적인 통신을 위한 기준 전압을 맞춰주는 선 (필수)
    - 한쪽의 `TX`는 다른 쪽의 `RX`에 연결하여, 서로 데이터를 주고받을 수 있게 합니다.

- **ESP32‑S3 보드** (Geekble nano ESP32‑S3)
  - **핵심 기능: 중앙 처리 장치 (두뇌)**
    - 이 프로젝트의 핵심 두뇌입니다. 두 가지 다른 통신 방식을 중개하고, 수신한 데이터를 가공하여 새로운 형태의 신호로 만들어 내보내는 모든 처리를 담당합니다.
  - **프로젝트 내 역할**
    - **입력 처리**: `USB-Serial 어댑터`로부터 UART 신호 형태로 마우스 데이터를 수신합니다.
    - **데이터 변환**: 수신한 데이터를 PC가 이해할 수 있는 표준 **USB HID(Human Interface Device) 마우스 프로토콜** 형식으로 변환합니다.
    - **출력 처리**: 변환된 HID 신호를 자신의 USB 포트를 통해 PC로 전송합니다. PC는 이 보드를 일반적인 USB 마우스로 인식하게 됩니다.

- **충전기** (5V 2A 이상 권장)
  - **사양**: USB-C PD 지원 권장 (30-60W)
  - **역할**: OTG Y 케이블의 Power 포트에 연결하여 스마트폰 충전
  - **주의사항**: 일부 스마트폰은 OTG 모드에서 충전 아이콘이 표시되지 않을 수 있으나 정상입니다.

- **USB 케이블** (ESP32‑S3↔PC)
  - **유형**: USB-C to USB-A 데이터 케이블 (충전 전용 케이블 사용 금지)
  - **역할**: ESP32-S3 보드에 전원을 공급하며, HID 마우스 신호를 PC에 전달
  - **주의사항**: 반드시 데이터 통신이 가능한 케이블을 사용해야 합니다.

---

## 2. 시스템 아키텍처

연결 구조:
```
스마트폰 ─USB-C─> OTG Y 케이블 ┬─Device포트─> USB-Serial ─UART─> ESP32-S3 ─USB─> PC
                              └─Power포트─> 충전기(PD)
   앱(8B 델타)                     TX/RX/GND           HID Boot Mouse    표준 HID
```

- **스마트폰**: OTG Y 케이블을 통해 Host 모드로 동작하며 동시에 충전
- **USB-Serial**: OTG Y 케이블의 Device 포트에 연결되어 USB↔UART 변환
- **ESP32-S3**: UART로 수신한 8바이트 프레임을 USB HID Boot Mouse로 변환
- **PC**: 전 부팅 단계(BIOS/BitLocker/UAC)에서 표준 마우스로 인식

지연 예측(참고): UART 직렬화 ≈ 0.08 ms(1 Mbps, 8B) + USB HID 폴링 1 ms → 체감 1–2 ms.

### 2.1 엔드투엔드 경로/신뢰성 가정
- 연결 경로: 유선(USB‑OTG) 우선. 본 문서는 USB 경로를 표준으로 정의합니다.
- 신뢰성 가정: PC 보안/부팅 단계에서 HID Boot Mouse로 인식되어야 하며, 앱/동글 분리 시에도 안전 정지(중립 프레임)가 보장되어야 합니다.
- 성능 목표: 전송 주기 4–8 ms(125–250 Hz), 엔드투엔드 입력 지연 50 ms 이하.

### 2.2 상태머신/핸드셰이크
- TransportState: {NoTransport, UsbOpening, UsbReady}
- BootSafe ↔ Normal 전환: 동글이 호스트의 HID `SET_PROTOCOL=REPORT` 전환을 감지하면 Normal로 간주. 그 전은 BootSafe로 동작.
- Keep‑alive: 앱→동글 5초 주기. 누락 3회 시 연결 불안정으로 간주하고 재연결 백오프(1s→2s→4s→8s) 적용.

### 2.3 성능/지연 측정 방법
- 타임스탬프 기반 RTT: 앱 송신 시퀀스 번호(`seq`)와 동글 회신 로그 비교.
- 오실로스코프: 터치 이벤트 → UART TX 라인 → USB SOF 간 지연 샘플링.
- 목표: P95 < 20 ms(펌웨어/링크), 앱 포함 E2E < 50 ms.

---

## 3. 프로토콜

> 본 절은 요약입니다. 규범적 상세 명세는 `Docs/technical-specification.md`의 프로토콜 섹션을 단일 출처로 참조하십시오.

중앙 상수 표 참조: `Docs/technical-specification.md` §1.1(상수/임계값 표) — `FRAME_SIZE_BYTES`, `TX_PERIOD_MS`, `KEEP_ALIVE_INTERVAL_MS`, `HID_SPLIT_DELTA_LIMIT` 등.

프레임 형식은 아래와 같습니다. 모든 필드는 Little‑Endian 기준입니다.

| 바이트 | 필드    | 형식  | 설명                                  |
|------:|---------|-------|---------------------------------------|
| 0     | seq     | u8    | 순번(유실/역전 로깅용)                |
| 1     | buttons | u8    | bit0 L, bit1 R, bit2 M                |
| 2..3  | dx      | i16   | 상대 X                                |
| 4..5  | dy      | i16   | 상대 Y                                |
| 6     | wheel   | i8    | 휠(부트 단계 비사용, OS 진입 후 선택) |
| 7     | flags   | u8    | 예약                                   |

- 전송 주기: 4–8 ms(125–250 Hz) 권장
- 큰 델타는 동글(ESP32‑S3)에서 8비트 한계(−127…+127)에 맞게 분할 처리(클리핑 방지)

### 3.1 `seq`/유실 판정 및 재동기화
- `seq`는 0..255 래핑. 수신측은 `(seq - prev_seq) mod 256`이 1이 아니면 유실/역전으로 로깅합니다.
- 재동기화(resync): UART 스트림에서 8바이트 정렬이 깨질 수 있으므로 8바이트 윈도우로 전진하며 유효성(keep‑alive 또는 합리적 델타) 검사를 통과하는 경계에서 재정렬합니다.

### 3.2 `flags` 비트 정의
- bit0 `IS_KEEPALIVE`: 헬스체크 프레임(버튼/델타 0). 수신측은 상태만 갱신.
- bit1 `EMERGENCY_STOP`: 즉시 모든 버튼 Up, 이동 0으로 중립화.
- bit2 `ACTIVE_CURSOR_ID`: 0=A, 1=B(멀티 커서 전환 힌트).
- bit3 `RESERVED_FOR_SCROLLING`: 무한 스크롤 내부 상태 힌트(선택).
- bit4 `RESERVED_RIGHT_ANGLE_MOVE`: 축 스냅 활성 힌트(선택).
- bit5..7 예약.

---

## 4. 하드웨어 연결

### 4.1 스마트폰 측 연결

#### 확정 구성: OTG Y 케이블 연결
1. **스마트폰 ↔ OTG Y 케이블**: USB-C 플러그를 스마트폰에 연결
2. **OTG Y 케이블 Device 포트 ↔ USB-Serial 어댑터**: 데이터 통신용
3. **OTG Y 케이블 Power 포트 ↔ 충전기**: 5V 2A 이상 (PD 30-60W 권장)

#### 연결 확인 사항:
- OTG Y 케이블의 Device/Power 포트를 혼동하지 않도록 라벨 확인
- 충전기 연결 후에도 OTG 기능이 정상 동작하는지 확인
- Android 앱에서 USB-Serial 어댑터가 정상 인식되는지 확인

### 4.2 ESP32-S3 측 연결 (UART 배선)

#### 점퍼 케이블 연결:
- **USB-Serial TX → ESP32-S3 RX** (예: GPIO17)
- **USB-Serial RX → ESP32-S3 TX** (예: GPIO18)  
- **USB-Serial GND ↔ ESP32-S3 GND** (공통 접지 필수)
- **VCC 미연결**: 전원 충돌 방지를 위해 연결하지 않음

#### 기술 사양:
- **UART 포트**: UART1 사용 (UART0는 플래싱/디버그용으로 예약)
- **시리얼 설정**: 1,000,000 bps, 8N1, No parity, No flow control
- **양방향 통신**: Android ↔ PC 간 확장 기능을 위한 양방향 데이터 전송 지원

### 4.3 전체 연결 다이어그램

```
[스마트폰]
    │
[OTG Y 케이블]
    ├─Device포트─[USB-Serial]─┬─TX──→RX─┐
    │                         ├─RX←──TX─┤ [ESP32-S3]──USB──[PC]
    │                         └─GND──GND─┘
    └─Power포트───[충전기]
```

---

## 5. ESP32‑S3 펌웨어(핵심 개념)

목표는 TinyUSB로 HID Boot Mouse를 구성하고, 1 ms 간격에서 USB를 서비스하면서 UART로 들어오는 8바이트 프레임을 수신 즉시 변환해 `tud_hid_report()`로 주입하는 것입니다.
**또한, PC의 Windows 서비스와 양방향 통신을 위해 Vendor-Defined 또는 CDC-ACM 인터페이스를 함께 구성해야 합니다.**

### 5.1 HID Report Descriptor(부트 마우스)

3버튼 + X/Y 상대 이동만 포함된 Boot Protocol 호환 디스크립터를 사용합니다. OS 단계에서 휠이 필요하면 Report Protocol 확장으로 별도 리포트를 추가할 수 있습니다.

#### 5.1.1 Boot ↔ Report 프로토콜 전환
- 동글은 호스트의 `SET_PROTOCOL(BOOT|REPORT)` 요청을 감지해 현재 프로토콜 상태를 유지해야 합니다.
- BOOT 모드: wheel=0 강제, 3버튼+X/Y만 보고.
- REPORT 모드: 별도 리포트 또는 확장 필드로 휠 보고 허용.

### 5.2 전송 루프 설계

- FreeRTOS 주기 태스크(1 ms)에서 `tud_task()` 호출 유지
- UART 수신 버퍼 크기는 8바이트 단위, 부분 수신은 다음 주기로 보완
- 입력 우선순위: Click > Wheel > Move

#### 5.2.1 재동기화 및 분할 주입
- 8바이트 정렬이 깨진 경우 1바이트씩 전진하며 유효 경계에서 재정렬(§3.1).
- `i16` 델타는 −127..127 범위로 분할하여 다중 HID 리포트로 주입(§5.3).

---

## 6. ESP32‑S3 구현 가이드(ESP‑IDF/TinyUSB, 공식 문서 준수)

### 6.1 핵심 공식 API(이름 기준)
- TinyUSB(Device): `tusb_init`, `tud_task`, `tud_ready`, `tud_hid_report`
- ESP‑IDF UART: `uart_driver_install`, `uart_param_config`, `uart_set_pin`, `uart_read_bytes`, `uart_write_bytes`
- FreeRTOS: `xTaskCreatePinnedToCore`, `vTaskDelay`, `xQueueReceive`

### 6.2 설계-구현 매핑 요약
- 루프 구조: 1 ms 주기 태스크에서 `tud_task()`를 호출하고, 별도 UART 폴링 태스크에서 8B 프레임을 수신하여 즉시 HID 주입.
- 버퍼링: 8바이트 고정 프레임 큐(깊이 ≥ 4). 부분 수신은 다음 주기로 보완, 정렬 손상 시 1바이트씩 전진하며 재정렬.
- 우선순위: 버튼(Down/Up) → 휠 → 이동 델타 순으로 처리. EMERGENCY_STOP은 즉시 중립화.
- 프로토콜 전환: `SET_PROTOCOL=REPORT` 감지 시 내부 상태를 Normal로 전환하여 휠 허용.

### 6.3 상태/이벤트 처리 표준
- Keep‑alive 수신: 내부 타임스탬프 갱신, 출력 없음.
- 유효 델타 수신: §5.3 규칙으로 분할 주입. 프레임 간 1 ms 이상 지연 시 마지막 프레임 우선 적용(coalescing).
- 시퀀스 역전/유실: 카운터 증가 및 디버그 로그. 안전 동작 지속.
- UART 에러/오버런: 드라이버 플러시 후 재초기화, 마지막 출력은 중립.

### 6.4 참조(공식 API 이름)
- TinyUSB: `tusb_init`, `tud_task`, `tud_hid_report`, `tud_hid_ready`
- ESP‑IDF: `uart_driver_install`, `uart_param_config`, `uart_set_pin`, `uart_read_bytes`
- FreeRTOS: `xTaskCreatePinnedToCore`, `vTaskDelay`

> 본 가이드는 코드가 아닌 구현 지침입니다. 실제 코드는 각 공식 레퍼런스를 따르십시오.

### 6.5 Geekble nano ESP32‑S3 핀맵/전기 설정(레퍼런스)

- UART 포트: `UART_NUM_1`
- 보레이트: `1_000_000 bps`(권장)
- 핀맵(권장):
  - `UART1 RX ← 어댑터 TX`: `GPIO17`
  - `UART1 TX → 어댑터 RX`: `GPIO18`
  - `GND`: 공통 접지 필수
- 주의: `UART0`는 플래싱/로그에 사용되므로 사용하지 않음. 전원은 각 기기 USB로 개별 공급, VCC 미연결.

### 6.6 ESP‑IDF 프로젝트 구조(예시)

```
firmware/
  CMakeLists.txt
  sdkconfig (또는 sdkconfig.defaults)
  main/
    CMakeLists.txt
    app_main.c
    uart_bridge.c/.h
    hid_injector.c/.h
    protocol.h // 8B 프레임 정의/상수
```

- 컴포넌트: TinyUSB(Device) 활성화, USB OTG 지원 옵션 사용(§12.1 참조).
- 로그 레벨: 개발 중 `INFO` 이상, 성능 측정 시 `WARN` 이하 권장.

### 6.7 초기화 시퀀스(의사코드)

```c
void app_main(void) {
  usb_init_tinyusb();        // tusb_init(); 디스크립터 등록
  uart_bridge_init(1_000_000, GPIO17, GPIO18);
  create_usb_task_1ms();     // 1 ms 주기 tud_task()
  create_uart_poll_task();   // 8B 프레임 수신 → 즉시 주입
}
```

### 6.8 UART 8B 프레이밍/재동기화(의사코드)

```c
bool read_8b_frame(uint8_t out[8]) {
  // 링버퍼에서 8바이트 시도 → 유효성 검사 실패 시 1바이트 전진
  while (rb_available() >= 8) {
    rb_peek(8, out);
    if (is_valid_frame(out)) { rb_drop(8); return true; }
    rb_drop(1);
  }
  return false;
}

bool is_valid_frame(const uint8_t f[8]) {
  // keep‑alive 또는 합리적 dx/dy 범위(절대치 클리핑 전)인지 검사
  if (f[7] & IS_KEEPALIVE) return true;
  int16_t dx = (int16_t)((f[3]<<8)|f[2]);
  int16_t dy = (int16_t)((f[5]<<8)|f[4]);
  return abs(dx) < 32768 && abs(dy) < 32768;
}
```

### 6.9 델타 분할 주입 규칙(의사코드)

```c
void inject_split_delta(const uint8_t f[8]) {
  // 우선순위: 버튼 → 휠 → 이동
  uint8_t buttons = f[1];
  int16_t dx = (int16_t)((f[3]<<8)|f[2]);
  int16_t dy = (int16_t)((f[5]<<8)|f[4]);
  int8_t  wheel = (int8_t)f[6];

  // 버튼 즉시 반영
  hid_inject(buttons, 0, 0, 0);

  // 휠(Report 모드에서만 허용)
  if (is_report_protocol() && wheel != 0) {
    hid_inject(buttons, 0, 0, wheel);
  }

  // 이동: −127..127로 분할 주입
  while (dx != 0 || dy != 0) {
    int8_t sx = (dx > 127) ? 127 : (dx < -127 ? -127 : (int8_t)dx);
    int8_t sy = (dy > 127) ? 127 : (dy < -127 ? -127 : (int8_t)dy);
    hid_inject(buttons, sx, sy, 0);
    dx -= sx; dy -= sy;
  }
}
```

### 6.10 SET_PROTOCOL 처리(부트/리포트 전환)

```c
volatile bool g_report_protocol = false;

uint8_t tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {
  g_report_protocol = (protocol != HID_PROTOCOL_BOOT);
  return 0; // OK
}

bool is_report_protocol(void) { return g_report_protocol; }
```

- BOOT 모드: wheel 강제 0, 3버튼+X/Y만 보고.
- REPORT 모드: 휠 보고 허용(별도 리포트 또는 확장 필드).

### 6.11 빌드/플래시 절차(ESP‑IDF)

```
idf.py set-target esp32s3
idf.py build
idf.py -p COMx -b 460800 flash monitor   # Windows, 포트 확인 필수
```

- 전원/케이블: 데이터 가능한 USB‑C 케이블 사용, 허브 품질에 따라 모니터 속도 조정.
- 모니터 종료: `Ctrl-]`.

### 6.12 성능/로깅 파라미터(권장)

- UART: 1,000,000 bps, RX 버퍼 ≥ 256B, 링버퍼/큐 깊이 ≥ 4프레임.
- USB: 1 ms SOF 주기 유지(`tud_task()` 1 ms).
- 로깅: 유실/역전 카운터, 부분 읽기/정렬 실패 카운터, 주입 프레임 수.
- 오류 시 복구: UART 드라이버 재초기화 후 마지막 출력은 중립 프레임.

---

## 7. Android(USB‑Serial) 송신 가이드

코드 예제 대신 실행 가능한 구현 지침을 제공합니다.

### 7.1 핵심 API/요건(이름 기준)
- 권한/장치: `UsbManager`, `UsbDevice`, `UsbInterface`, `UsbEndpoint`, `UsbDeviceConnection`
- I/O: `UsbDeviceConnection.bulkTransfer`
- 타이밍: `Choreographer` 또는 전용 스케줄러(4–8 ms 주기)

### 7.2 전송 루프/레이트 컨트롤
- 주기: 4–8 ms. 프레임 생성 시 가장 최근 입력을 반영하되, 동일 주기 다중 이벤트는 마지막 상태로 코얼레싱.
- 부분 쓰기/타임아웃: `bulkTransfer` 타임아웃을 2–4 ms로 설정. 부분 쓰기 발생 시 1회 재시도 후 다음 주기로 승계.

### 7.3 Keep‑alive/헬스체크
- 5초 주기로 keep‑alive(§3.2 `IS_KEEPALIVE`) 송신. 응답 누락 3회 시 재연결 플로우로 전이.

### 7.4 오류/복구
- 권한 소실/장치 분리: 즉시 중립 프레임 2–3회 송신 후 포트 닫기 → 백오프(1s→2s→4s→8s)로 재시도.
- 포트 오픈 실패: 최대 10초 타임아웃 내 백오프 재시도. 실패 시 사용자 알림.

### 7.5 로깅/계측
- `seq`와 송신 타임스탬프 로깅, 유실/역전/부분 쓰기 카운터.

---

## 8. 하드웨어 연결 검증 가이드

### 8.1 단계별 연결 확인

#### 1단계: OTG Y 케이블 연결 확인
```
✓ 스마트폰에 OTG Y 케이블 연결
✓ Device 포트에 USB-Serial 어댑터 연결  
✓ Power 포트에 충전기 연결
```

**확인 방법:**
- Android 알림창에서 "USB 액세서리 연결됨" 또는 "OTG 장치 연결" 메시지 확인
- 설정 > 연결된 장치에서 USB 장치 인식 여부 확인
- 충전 표시등 또는 배터리 아이콘 변화 확인 (일부 기기는 변화 없을 수 있음)

#### 2단계: USB-Serial 어댑터 인식 확인
**Android에서:**
```bash
# Android 터미널 앱 또는 개발자 도구에서
ls /dev/ttyUSB*
# 또는
lsusb
```

**확인되어야 할 것:**
- `/dev/ttyUSB0` (또는 유사한 장치 파일) 존재
- VID/PID: `10C4:EA60` (CP2102) 인식

#### 3단계: ESP32-S3 UART 통신 확인
**방법 1: 시리얼 모니터 사용**
```bash
# ESP32-S3를 PC에 연결 후
idf.py monitor
```

**방법 2: 간단한 에코 테스트**
```c
// ESP32-S3 펌웨어에 임시 추가
void uart_echo_test() {
    uint8_t data[8];
    int len = uart_read_bytes(UART_NUM_1, data, 8, 100);
    if (len > 0) {
        uart_write_bytes(UART_NUM_1, "OK\n", 3);
    }
}
```

#### 4단계: ESP32-S3 HID 장치 인식 확인
**Windows에서:**
1. 장치 관리자 (`devmgmt.msc`) 실행
2. "휴먼 인터페이스 장치" 섹션 확인
3. "HID-compliant mouse" 항목 존재 여부 확인

**확인되어야 할 것:**
- HID-compliant mouse (부트 마우스)
- USB 복합 장치 또는 직렬 포트 (CDC/Vendor 인터페이스)

#### 5단계: 전체 시스템 통신 테스트
**최종 검증:**
1. Android 앱에서 마우스 이동 명령 전송
2. PC에서 커서 이동 확인
3. 클릭 동작 확인

### 8.2 Android 앱에서 확인하는 방법

#### USB 장치 인식 확인
```java
// Android 앱에서 USB 장치 확인
UsbManager usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();
for (UsbDevice device : deviceList.values()) {
    if (device.getVendorId() == 0x10C4 && device.getProductId() == 0xEA60) {
        Log.d("USB", "CP2102 USB-Serial 어댑터 인식됨");
        // 권한 요청 및 연결 시도
    }
}
```

#### 데이터 전송 테스트
```java
// 간단한 테스트 프레임 전송
byte[] testFrame = {
    0x01,        // seq
    0x00,        // buttons (no click)
    0x10, 0x00,  // dx = 16 (작은 오른쪽 이동)
    0x00, 0x00,  // dy = 0
    0x00,        // wheel
    0x00         // flags
};

int bytesWritten = connection.bulkTransfer(endpoint, testFrame, 8, 1000);
if (bytesWritten == 8) {
    Log.d("USB", "테스트 프레임 전송 성공");
} else {
    Log.e("USB", "전송 실패: " + bytesWritten + " bytes");
}
```

### 8.3 문제 발생 시 단계별 진단

#### 문제: OTG 장치가 인식되지 않음
**확인 사항:**
1. OTG Y 케이블의 Device/Power 포트 올바른 연결
2. 스마트폰 OTG 기능 활성화 (설정 > 연결된 장치 > OTG)
3. USB 디버깅 활성화
4. "USB를 통한 전원 공급" 설정 확인

#### 문제: ESP32-S3가 HID로 인식되지 않음
**확인 사항:**
1. ESP32-S3 전원 공급 (LED 점등 확인)
2. USB 케이블이 데이터 전송 가능한지 확인
3. TinyUSB 설정에서 HID 활성화 여부
4. 펌웨어 정상 플래시 여부

#### 문제: UART 통신이 안됨
**확인 사항:**
1. TX-RX 크로스 연결 (TX ↔ RX)
2. GND 공통 접지 연결
3. VCC 미연결 상태 확인
4. 보레이트 설정 일치 (1,000,000 bps)

### 8.4 성공 지표

✅ **모든 연결이 성공했다면:**
- Android: USB 장치 목록에 CP2102 표시
- ESP32-S3: 시리얼 모니터에서 UART 데이터 수신 로그
- Windows: 장치 관리자에 "HID-compliant mouse" 표시
- 최종: Android 터치/드래그 → PC 커서 이동

---

## 9. 빠른 시작

### 준비 단계
1. **OTG Y 케이블 연결**:
   - 스마트폰에 OTG Y 케이블 연결
   - Device 포트에 USB-Serial 어댑터 연결
   - Power 포트에 충전기 연결 (5V 2A 이상)

2. **UART 배선**:
   - USB-Serial TX → ESP32-S3 RX (GPIO17)
   - USB-Serial RX → ESP32-S3 TX (GPIO18)
   - USB-Serial GND ↔ ESP32-S3 GND

### 펌웨어 설정
1. ESP-IDF로 TinyUSB **복합 장치(HID + Vendor/CDC)** 예제 빌드
2. ESP32-S3에 플래시 후 PC에 연결
3. 장치 관리자에서 "HID-compliant mouse" 및 "직렬 포트(COMx)" 확인

### 동작 테스트
1. **Android → PC**: 
   - Android 앱에서 USB-Serial 어댑터 인식 확인
   - 8바이트 마우스 프레임 전송
   - PC에서 커서 이동/클릭 동작 확인

2. **PC → Android** (양방향 통신):
   - PC 터미널(PuTTY 등)에서 ESP32-S3의 COM 포트 연결
   - 데이터 송신 후 Android 앱에서 수신 확인

### 확인 사항
- OTG 모드 정상 동작 (USB 장치 인식)
- 충전 동시 진행 (배터리 소모 방지)
- 양방향 데이터 통신 무결성

---

## 10. 테스트 체크리스트

### OTG Y 케이블 연결 테스트
- [ ] OTG Y 케이블 정상 인식 (Device/Power 포트 구분)
- [ ] 충전과 OTG 동시 동작 확인
- [ ] USB-Serial 어댑터 안정적 인식
- [ ] 장시간(≥8h) 연속 사용 시 연결 안정성

### HID 마우스 기능 테스트
- [ ] BIOS/UEFI 환경에서 마우스 동작
- [ ] BitLocker PIN 입력 화면에서 동작
- [ ] Windows 로그인 화면에서 동작
- [ ] UAC Secure Desktop에서 동작
- [ ] 좌/우/중간 버튼 클릭 정상 동작
- [ ] 커서 이동 정확도 및 반응 속도

### 성능 및 안정성 테스트
- [ ] 장시간(≥24h) 연속 송신 시 프레임 드랍/지터 없음
- [ ] 최대 델타 값 처리 (분할 주입 동작 확인)
- [ ] 긴급 정지 제스처 (양 버튼 동시 3초) 동작
- [ ] 재연결 시 안정적 복구

### 양방향 통신 테스트
- [ ] Android → PC: 마우스 데이터 전송
- [ ] PC → Android: Vendor/CDC 채널 데이터 수신
- [ ] 양방향 동시 통신 시 데이터 무결성
- [ ] 확장 기능 명령어 처리

### 전원 관리 테스트
- [ ] 충전 중 OTG 기능 유지
- [ ] 배터리 소모율 측정 (충전 없이 vs 충전 병행)
- [ ] 다양한 충전기 호환성 (5V 2A, PD 30W, PD 60W)

---

## 11. 트러블슈팅/가드레일

### OTG Y 케이블 관련 문제

#### USB-Serial 어댑터가 인식되지 않음
- **원인**: Device/Power 포트 혼동, 비호환 케이블
- **해결**: 
  - OTG Y 케이블의 포트 라벨 확인 (Device ↔ Power)
  - "Charge-while-OTG" 지원 케이블인지 확인
  - Android 설정에서 USB 디버깅/OTG 활성화
  - USB 권한 요청 다이얼로그 승인

#### 충전이 되지 않음 / 배터리 아이콘 변화 없음
- **원인**: 일부 스마트폰의 OTG 모드 제한
- **해결**:
  - 충전기 전력 확인 (최소 5V 2A, PD 30-60W 권장)
  - 배터리 아이콘이 변하지 않아도 실제 충전 중일 수 있음
  - 배터리 사용량 모니터링으로 확인
  - 다른 OTG Y 케이블로 교체 테스트

#### OTG 모드가 간헐적으로 끊김
- **원인**: 불안정한 연결, 전력 부족
- **해결**:
  - 모든 연결부 재확인 (특히 USB-C 커넥터)
  - 더 높은 전력의 충전기 사용
  - 케이블 길이 최소화
  - USB-Serial 어댑터 교체 (CP2102 권장)

### ESP32-S3 관련 문제

#### HID 마우스로 인식되지 않음
- **해결**:
  - TinyUSB 복합 장치 설정 확인
  - Boot Mouse 디스크립터 올바른지 확인
  - `tud_task()` 1ms 주기 호출 확인
  - Windows 장치 관리자에서 "HID-compliant mouse" 확인

#### UART 통신 오류
- **해결**:
  - 보레이트 설정 확인 (1,000,000 bps)
  - GND 공통 접지 연결 확인
  - TX-RX 크로스 연결 확인
  - VCC 미연결 상태 확인

### 정책 및 보안
- **기업 단말**: USB Host 권한 정책 확인
- **장치 분리 시**: 즉시 중립 프레임 전송 후 재연결
- **펌웨어 업데이트**: 안전 초기 상태로 시작
