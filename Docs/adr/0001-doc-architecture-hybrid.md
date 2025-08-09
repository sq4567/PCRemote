---
title: "ADR-0001: 문서 아키텍처 — 하이브리드 구조 채택"
status: "Accepted"
date: "2025-08-13"
deciders: ["Architecture", "Android", "Firmware", "Windows"]
tags: ["adr", "documentation", "governance", "spec"]
---

## 배경(Context)

PCRemote는 여러 문서가 상호 참조하는 구조입니다: `Docs/design-guide-app.md`, `Docs/touchpad.md`, `Docs/component-design-guide.md`, `Docs/usb-hid-bridge-architecture.md`, 스타일프레임 문서들. 이들 문서에는 플랫폼을 가로지르는 교차 계약(프로토콜, 상태 모델, 제스처/알고리즘, 성능/KPI, 오류/폴백)이 포함되거나 의존합니다.

교차 계약이 여러 문서에 분산되면 다음 문제가 반복됩니다.
- 계약 불일치(드리프트)와 모순 탐지 비용 증가
- 변경 책임 소재 불명확, 링크/앵커 붕괴
- 온보딩 난이도 증가, 리뷰 기준 모호화

동시에 플랫폼별 구현(Android/ESP32‑S3/Windows)은 API/빌드/권한 등 로컬 특성이 강해, 중앙 명세에 모든 구현 세부를 수용하면 문서 비대화와 변경 충돌이 발생합니다.

## 결정(Decision)

문서 아키텍처를 하이브리드로 채택합니다.

- 중앙 교차 명세서(`Docs/technical-specification.md`)를 Single Source of Truth(SSOT)로 지정한다.
  - 규범적 범위: 프로토콜(8B 프레임/flags/분할 주입/keep‑alive), 상태 모델(BootSafe↔Normal), 제스처/모드/옵션 알고리즘, 성능 KPI/측정법, 오류/폴백, 플랫폼 간 계약(규범).
  - 거버넌스: 교차 계약 변경은 반드시 중앙 명세를 선행 갱신하고, 후속으로 플랫폼 문서들을 정합화한다.
- 플랫폼별 문서에는 비규범(구현 방법/예시/빌드/권한/OS 제약)을 유지한다.
  - Android: Compose 패턴, 권한/OTG/서비스 등 구현 가이드(신규 `Docs/android-implementation.md` 예정)
  - ESP32‑S3: `Docs/usb-hid-bridge-architecture.md`(ESP‑IDF/TinyUSB, 핀/루프/에러 처리)
  - Windows: `Docs/windows-server.md`(매크로/입력 주입/권한/테스트)

## 의사결정 동인(Decision Drivers)

- 교차 계약 단일화로 드리프트 최소화
- 유지보수/확장성(문서 비대화 억제, 역할 분리)
- 리뷰/PR 게이트 명확화(규범 변경 → 중앙 명세 우선)
- 온보딩/검색 효율(중앙 명세를 첫 진입점으로)

## 고려 대안(Options Considered)

1) 전면 중앙집중(모든 내용을 `technical-specification.md`로 수용)
- 장점: 단일 진실원, 일관성 최고
- 단점: 문서 비대화, 플랫폼별 API/빌드 내용과 혼재, 변경 충돌/가독성 저하

2) 하이브리드(선택안)
- 장점: 규범만 중앙, 구현은 분리 → 균형적 유지보수/확장성
- 단점: 교차 링크/역할 경계 관리가 필요(운영 규칙 필수)

3) 완전 분산(중앙 명세 없음)
- 장점: 초기 속도, 문서 경량
- 단점: 교차 계약 불일치 위험 상시, 모순 해결 비용↑

## 결과/영향(Consequences)

긍정적:
- 규범 변경 경로가 단순화(중앙 → 플랫폼 전파)
- 용어/상수/상태/프로토콜의 일관성 향상
- 리뷰 기준 표준화(RFC2119, 섹션/앵커 규칙)

부정적/비용:
- 초기 정리(교차 링크/앵커/상수표 통합) 작업 필요
- 운영 규칙 위반 시 다시 드리프트 발생 가능 → PR 게이트로 보완

## 범위(Scope) / 비범위(Out of Scope)

- 범위: 문서 구조/거버넌스, 규범/비규범 분리 원칙
- 비범위: 개별 기능의 세부 알고리즘 설계(중앙 명세 본문에서 다룸)

## 운영 원칙(Governance)

- PR 체크리스트(요지):
  - 교차 계약 변경 시 `Docs/technical-specification.md` 섹션 링크 포함 MUST
  - 플랫폼 문서 변경 시 중앙 명세와의 정합성 점검 MUST
  - 섹션 번호/앵커, RFC2119, 용어 표(`Selected/Unselected`, `Enabled/Disabled`) 준수 SHOULD
- 링크/앵커: 상대경로+§번호 앵커 사용, 깨진 앵커 방지

## 롤아웃 계획(Rollout)

1) 중앙 명세 v0 스켈레톤 작성(완료)
2) `touchpad.md`에서 알고리즘 참조를 중앙 명세 앵커로 연결(완료)
3) `usb-hid-bridge-architecture.md` 프로토콜 절에 중앙 명세 참조 문구 추가(완료)
4) `windows-server.md` 초안 작성(완료), API 매핑/연동 플로우는 후속
5) `component-design-guide.md` §2.2/§2.4 정책을 중앙 명세 §8과 상호 링크(후속)

## 리스크 및 대응(Risks & Mitigations)

- 위반 PR 유입: PR 템플릿/리뷰 가이드 도입
- 링크 부식: 정기 앵커 검사 To‑Do 유지
- 규범/비규범 경계 혼동: 중앙 명세에 “포함/제외” 표 유지

## 성공 기준(Validation)

- 교차 문서 상충/모순 체크리스트가 0건으로 유지
- 신규 기능 PR에 중앙 명세 섹션 링크 100% 포함
- 릴리즈 전 링크/앵커 검사 통과

## 참고(References)

- `Docs/technical-specification.md`
- `Docs/touchpad.md`
- `Docs/usb-hid-bridge-architecture.md`
- `Docs/component-design-guide.md`
- `Docs/windows-server.md`


