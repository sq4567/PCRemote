# PCRemote - 접근성 우선 PC 원격 제어 솔루션

## 🎯 프로젝트 목적

**PCRemote**는 근육장애로 인해 기존 키보드/마우스 사용이 어려운 사용자들을 위한 **접근성 우선 Android-PC 입력 브릿지**입니다.

### 핵심 설계 원칙
- **🤏 단일 터치**: 모든 기능을 한 번의 터치로 수행
- **🎯 컴팩트 레이아웃**: 중앙 하단 240×280dp 영역에 모든 조작 집중
- **🤚 한손 조작**: 엄지손가락만으로 완전한 PC 제어
- **⚡ 즉시 사용**: 복잡한 설정 없이 연결만 하면 바로 사용 가능
- **🔄 안정성**: 작업 중 연결 끊김으로 인한 스트레스 방지

---

## 🏗️ 프로젝트 구조

```
PCRemote/
├── Android/              # Android 클라이언트 앱 (Kotlin)
│   ├── app/src/main/java/com/pcremote/
│   │   ├── app/          # 메인 앱 구조
│   │   ├── domain/       # 비즈니스 로직
│   │   └── presentation/ # UI 및 ViewModel
│   └── build.gradle.kts
├── Windows/              # Windows 서버 (C#)
│   ├── PCRemoteServer/
│   │   ├── Services/     # 서버 핵심 서비스
│   │   ├── Models/       # 통신 메시지 모델
│   │   └── Views/        # 시스템 트레이 UI
│   └── README.md
└── Docs/                 # 프로젝트 문서
    ├── prd.md           # 제품 요구사항 명세서
    ├── technical-specification.md
    ├── design-guide-app.md      # 앱 디자인 가이드
    ├── component-design-guide.md # 컴포넌트 디자인 상세 명세
    └── styleframe-*.md          # 페이지별 스타일프레임
```

---

## 🚀 주요 기능

### 접근성 특화 기능
- **터치패드 모드**: 단일 터치로 정밀한 커서 제어
- **탭 안정화**: 손떨림 방지 알고리즘으로 정확한 클릭
- **키보드 모드**: 자주 사용하는 단축키 큰 버튼으로 제공
- **페이지 슬라이드**: 여백 영역 슬라이드로 직관적 모드 전환
- **길게 누르기**: 우클릭, 드래그 등을 위한 대체 입력 방식

### 핵심 기능
- **🖱️ 마우스 제어**: 이동, 좌/우클릭, 스크롤, 드래그
- **⌨️ 키보드 제어**: 단축키 (Ctrl+C/V, Alt+Tab, Space, Enter)
- **🔌 USB ADB 연결**: USB 케이블을 통한 안정적이고 빠른 연결 (Primary)
- **🔍 Wi-Fi 백업 연결**: USB 사용 불가 시 WiFi 네트워크 자동 검색 (Backup)
- **⚡ 초저지연 통신**: USB ADB 기반 10ms 이하 입력 지연
- **📱 시스템 트레이**: Windows 백그라운드 실행

---

## 💻 시스템 요구사항

### Android 클라이언트
- **OS**: Android 8.0 (API 26) 이상
- **연결**: USB 케이블 (Primary) + WiFi (Backup)
- **USB 지원**: USB Host API 지원
- **접근성**: 단일 터치 입력 가능

### Windows 서버
- **OS**: Windows 10/11 (64-bit)
- **런타임**: .NET 6.0
- **연결**: USB ADB (Primary) + WiFi/이더넷 (Backup)
- **포트**: USB ADB 5555 (Primary), 8080 WebSocket (Backup), 8081 UDP Discovery (Backup)

---

## 🔧 설치 및 실행

### Windows 서버 설치
```bash
# 1. .NET 6.0 Runtime 설치 (필요한 경우)
# https://dotnet.microsoft.com/download/dotnet/6.0

# 2. ADB Tools 설치 (USB ADB 통신용)
# Android SDK Platform Tools 또는 ADB만 별도 설치
# https://developer.android.com/studio/releases/platform-tools

# 3. 프로젝트 빌드
cd Windows/PCRemoteServer
dotnet restore
dotnet build -c Release

# 4. 서버 실행
dotnet run -c Release

# 5. 실행 파일 생성 (배포용)
dotnet publish -c Release -r win-x64 --self-contained
```

### Android 앱 설치
```bash
# 1. Android Studio에서 프로젝트 열기
cd Android

# 2. Gradle 동기화
./gradlew sync

# 3. 디바이스에 설치
./gradlew installDebug
```

---

## 🎮 사용법

### 1. USB ADB 연결 (권장 - Primary)
1. Windows PC에서 `PCRemoteServer.exe` 실행
2. Android 디바이스에서 **USB 디버깅 활성화**
   - 설정 → 휴대전화 정보 → 빌드 번호 7회 터치 (개발자 옵션 활성화)
   - 설정 → 개발자 옵션 → USB 디버깅 ON
3. USB 케이블로 Android와 PC 연결
4. USB 디버깅 허용 팝업에서 "확인"
5. PCRemote 앱 실행 → 자동으로 USB 연결됨

### 2. Wi-Fi 백업 연결 (USB 사용 불가 시)
1. Windows PC에서 `PCRemoteServer.exe` 실행
2. 시스템 트레이에서 서버 상태 확인 (WebSocket 백업 서버 시작됨)
3. Android와 PC가 같은 Wi-Fi 네트워크 연결
4. Android 앱 실행 → "Wi-Fi 연결" 버튼으로 자동 서버 검색
5. 연결 성공 시 터치패드/키보드 모드 사용 가능

### 3. 접근성 조작 방법
- **터치패드 페이지**: 터치로 커서 이동, R버튼으로 우클릭
- **키보드 페이지**: 큰 버튼으로 주요 단축키 입력
- **설정 페이지**: 터치 감도, 버튼 크기 등 개인 맞춤화
- **페이지 전환**: 여백 영역에서 좌우 슬라이드

---

## 🏛️ 기술 스택

### Android (Kotlin)
- **UI**: Jetpack Compose
- **아키텍처**: MVVM + Clean Architecture
- **의존성 주입**: Hilt
- **통신**: USB ADB (Primary), WebSocket (Backup), UDP (Discovery)
- **비동기**: Coroutines + Flow
- **USB**: Android USB Host API

### Windows (C#)
- **플랫폼**: .NET 6.0
- **UI**: WPF + 시스템 트레이
- **통신**: USB ADB (SharpAdbClient), WebSocket (Fleck), UDP
- **입력 제어**: Windows API (SendInput)
- **DI**: Microsoft.Extensions.DependencyInjection

---

## 📋 통신 프로토콜

### USB ADB 메시지 (Primary - Binary)
```bash
# ADB Shell Command 형태로 전송 (16ms 배치)
adb shell input_event:{"x":0.5,"y":0.3,"action":"LEFT_CLICK","timestamp":1640995200000}

# 터치 이벤트 배치 전송 (성능 최적화)
adb shell batch_input:[{"x":0.5,"y":0.3,"action":"MOVE"},{"x":0.5,"y":0.3,"action":"LEFT_CLICK"}]

# 키보드 이벤트
adb shell key_event:{"shortcutType":"CTRL_C","timestamp":1640995200000}
```

### WebSocket 메시지 (Backup - JSON)
```json
// 터치 이벤트 (Android → PC)
{
  "x": 0.5,
  "y": 0.3,
  "action": "LEFT_CLICK",
  "timestamp": 1640995200000
}

// 키보드 이벤트 (Android → PC)
{
  "shortcutType": "CTRL_C",
  "timestamp": 1640995200000
}

// 상태 응답 (PC → Android)
{
  "status": "SUCCESS",
  "message": "터치 이벤트 처리 완료"
}
```

---

## 🔒 보안 및 접근성

### 보안
- **USB 직접 연결**: 가장 안전한 물리적 연결 (Primary)
- **로컬 네트워크 전용**: Wi-Fi 백업 연결 시 같은 LAN에서만 동작
- **ADB 권한 관리**: Android USB 디버깅 권한 필요
- **메시지 검증**: 수신 데이터 형식 및 범위 검증
- **권한 최소화**: 표준 사용자 권한으로 실행

### 접근성 고려사항
- **큰 터치 영역**: 최소 80×60dp 버튼 크기
- **햅틱 피드백**: 모든 터치에 진동 응답
- **시각적 피드백**: 연결 상태 및 처리 결과 표시
- **개인 맞춤화**: 터치 감도, 버튼 크기 조절

---

## 🛣️ 개발 로드맵

### 🔄 Phase 1: 기본 USB 통신 & 접근성 UI (진행중)
- **USB ADB 통신 구현** (Primary Communication)
- 단일 터치 터치패드
- 기본 클릭 기능
- 접근성 최적화 UI 구조

### 📋 Phase 2: 단일 터치 입력 처리 (예정)
- **Wi-Fi 백업 통신 구현** (Secondary Communication)
- 모드 전환 시스템 (터치패드 ↔ 키보드)
- 스크롤 기능
- 주요 단축키 지원
- 10ms 이하 입력 지연 최적화

### 📋 Phase 3: 접근성 강화 (예정)
- 개인 설정 저장
- 음성 피드백
- 드래그 앤 드롭
- 매크로 기능

---

## 🤝 기여 방법

1. **이슈 제보**: 버그 발견이나 개선사항 제안
2. **접근성 피드백**: 실제 사용자 관점의 개선사항
3. **코드 기여**: 풀 리퀘스트를 통한 기능 개선
4. **문서 개선**: 사용법, 설치 가이드 개선
5. **테스트**: 다양한 환경에서의 호환성 테스트

---

## 📄 라이선스

이 프로젝트는 **접근성 향상**을 목적으로 개발되었으며, **MIT 라이선스** 하에 배포됩니다.

---

## 🙏 감사의 말

이 프로젝트는 신체적 제약으로 인해 기존 입력 장치 사용이 어려운 모든 사용자들이 더 자유롭게 컴퓨터를 사용할 수 있기를 바라는 마음으로 개발되었습니다.

**"기술은 모든 사람을 위한 것이어야 합니다"**

---

## 📞 지원 및 문의

- **GitHub Issues**: 버그 리포트 및 기능 요청
- **문서**: `/Docs` 폴더의 상세 기술 문서
  - `design-guide-app.md`: 앱 전체 디자인 가이드
  - `component-design-guide.md`: UI 컴포넌트 상세 명세
  - `styleframe-*.md`: 페이지별 스타일프레임
- **Windows 서버**: `/Windows/README.md` 참조

**PCRemote**와 함께 더 접근 가능한 디지털 환경을 만들어 나가요! 🌟 