# e-Paper Calendar — 작업 핸드오프

## 프로젝트 개요
- **기기**: LilyGo T5 4.7" EPD (ESP32-S3, OPI PSRAM 8MB)
- **해상도**: 960×540, 4-bit 그레이스케일 프레임버퍼
- **빌드**: PlatformIO, env `T5-ePaper-S3`
- **포트**: `/dev/cu.usbmodem1101`
- **업로드**: `~/.platformio/penv/bin/platformio run -e T5-ePaper-S3 --upload-port /dev/cu.usbmodem1101 -t upload`
- **목표**: Google Calendar 연동 e-paper 캘린더 (주간 뷰 + 일간 미팅 뷰 + 수면 화면)

---

## 파일 구조 (주요)

| 파일 | 역할 |
|------|------|
| `custom_calendar.ino` | 메인 루프, 프레임버퍼 관리, 모드 전환 |
| `layout_weekly.cpp/h` | 주간 뷰 레이아웃 |
| `layout_meeting.cpp/h` | 일간 미팅 뷰 레이아웃 |
| `render_utils.cpp/h` | 텍스트·도형 렌더링 유틸 |
| `google_calendar.cpp/h` | Google Calendar API 이벤트 fetch |
| `google_auth.cpp/h` | OAuth2 Device Flow 인증 |
| `config.h` | 전역 상수 (화면 크기, 타임그리드 등) |
| `calendar_data.h` | CalendarEvent/CalendarData 구조체 |
| `secrets.h` | WiFi/Google 인증 정보 (git 미포함) |
| `fonts/NotoSansKR_*.h` | 한국어 폰트 (8, 12, 16, 24, 36pt) |
| `cat_image.h` | 수면 화면 고양이 이미지 |

---

## 핵심 기술 상수 (config.h)

```c
SCREEN_W = 960, SCREEN_H = 540
HEADER_H = 38
WEEK_STRIP_Y = 40, WEEK_STRIP_H = 76
WEEK_COL_W = 137  (960/7)
TGRID_START_H = 9   // 타임그리드 시작 시간
TGRID_W_END_H = 23  // 주간 뷰 타임그리드 끝 (23:30까지)
TGRID_W_PX_HOUR = 72  // 시간당 픽셀
MEETING_SPLIT_X = 460  // 일간 뷰 좌우 분할선
MEETING_WARN_SECONDS = 600  // 10분 전부터 미팅 뷰 표시
```

---

## EPD 드라이버 핵심 버그 (수정 완료)

### `epd_draw_grayscale_image` 포인터 오프셋 버그
`src/epd_driver.c`의 `provide_out` 함수는 `area.y > 0`일 때 데이터 포인터를
`area.y * (EPD_WIDTH/2)` 만큼 앞으로 이동하지 않는다.
즉 `epd_draw_grayscale_image({y=100, h=50}, g_fb)`를 호출하면
`g_fb[row 0..49]`(헤더 영역)를 화면 row 100..149에 그린다.

**수정**: `flashArea()`에서 `g_fb + bandOffset`으로 포인터를 직접 오프셋해서 전달.

```cpp
// custom_calendar.ino — flashArea() 핵심 수정
epd_draw_grayscale_image(band, g_fb + bandOffset);  // 반드시 오프셋 포함
```

---

## 디스플레이 모드

| 모드 | 조건 | 화면 |
|------|------|------|
| `MODE_SLEEP` | 00:00~09:00 KST | 고양이 이미지 |
| `MODE_WEEKLY` | 기본 | 주간 7열 타임그리드 |
| `MODE_MEETING` | 이벤트 10분 전~종료 | 좌: 일간 타임라인 / 우: 이벤트 상세 |
| `MODE_ERROR` | 인증 실패 | — |

---

## 새로고침 전략

- **전체 화면**: 모드 전환 시 4사이클 clear
- **정기 화면 이동**: 주간/일간 타임라인은 30분 슬롯 변경 시에만 재그리기
- **부분 플래시**: 현재 시간 바와 분 단위 카운트다운 플래시는 제거됨
- **데이터 fetch**: Google Calendar 데이터는 1분마다 갱신하되, 화면 redraw는 모드 전환/30분 슬롯 변경 기준

---

## 가상 좌표 시스템 (주간·일간 공통)

09:00 = vY 0, 23:30 = vY 1044 (= 14.5시간 × 72px/hr)

```
vY = (hour - 9) * 72 + min/60 * 72
scrollAnchor = floor(now to 00/30 minute slot)
scrollOffset = clamp(scrollAnchorVY - evAreaH/3, 0, VEND - evAreaH)
screenY = evAreaY + vY - scrollOffset
```

주간 뷰: `evAreaY = WEEK_STRIP_Y + WEEK_STRIP_H + 1 = 117`
일간 뷰 타임라인: `evAreaY = TLINE_TOP = 10`

---

## 일간 뷰 (layout_meeting.cpp) 레이아웃 Y 위치

### 오른쪽 패널 (MEETING_SPLIT_X=460 이후)
| 요소 | Y 위치 |
|------|--------|
| 배지 ("진행 중" / "곧 시작") | y=14, h=32, baseline=36 |
| 타이틀 (최대 2줄) | titleY=76, stopY=92 |
| 구분선 | nextY+4 |
| 시간 행 | nextY+22 (시간 범위만 표시, 분 단위 카운트다운 없음) |
| 장소 행 | +43px |
| 참석자 헤더 | +43px |
| 참석자 목록 | 각 39px 간격 |

### 왼쪽 패널 (타임라인)
- 그리드: 09:00~23:30, 30분 슬롯, 스크롤
- 이벤트 타이틀 baseline: `topY + FONT12_ASCENT + 17` (최소 높이 FONT12_ASCENT+19)
- 현재 시간 마커: 제거됨

---

## 참가자 표시 (google_calendar.cpp)

Google Calendar API에서 `displayName` 필드를 우선 사용.
- `self: true` 항목(본인)은 참석자 목록에서 제외 (API가 본인을 참석자로 포함)
- `displayName`에 '@' 포함 시(이메일이 displayName인 계정) → `email` 전체 표시
- Google API가 displayName 미제공 외부 참가자 → 이메일 주소 전체 표시 (API 한계)

---

## 알려진 제한 사항 / TODO

- [ ] 외부 Google 계정 참가자 이름 미제공 → 이메일 주소로 표시됨 (API 한계)
- [ ] 주간 뷰에서 같은 시간대 이벤트 여러 개 → 겹쳐 표시됨 (열 분할 미구현)
- [ ] 네트워크 오류 시 마지막 캐시 데이터 사용 (만료 표시 없음)

---

## 최근 변경 이력

| 날짜 | 변경 내용 |
|------|-----------|
| 2026-07-01 | e-ink 잔상 저감: 주간/일간 현재시간 바 제거, 우측 상단 시계 제거, 타임라인 이동 30분 단위화, 분 단위 카운트다운 플래시 제거 |
| 2026-04-23 | `flashArea` 포인터 버그 수정 (배지 중복 표시 근본 원인) |
| 2026-04-23 | 부분 클리어 2사이클, 전체 클리어 4사이클로 통일 |
| 2026-04-23 | 일간 뷰 타임라인 09:00~23:30 스크롤 구현 |
| 2026-04-23 | 일간 뷰 좌측 타임라인 타이틀 +15px 이동 |
| 2026-04-23 | 참가자 표시: displayName 우선, '@' 포함 시 이메일 전체 표시 |
| 이전 | 주간 뷰 09:00~23:30 스크롤, 이벤트 블록 흑백 반전, 폰트 축소 |
