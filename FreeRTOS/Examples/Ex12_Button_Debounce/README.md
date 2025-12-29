# Example 12: 버튼 디바운싱 데모

## 📌 학습 목표

- 버튼 바운싱 문제와 해결책 이해
- 상태 머신 기반 디바운스 구현
- 버튼 이벤트 (Press/Release/LongPress) 처리

---

## ⚡ 버튼 바운싱 문제

```mermaid
graph TB
    subgraph "물리적 스위치"
        SW["기계식 접점"]
    end
    
    subgraph "바운싱 발생"
        B1["접점 진동"]
        B2["10~50ms 지속"]
        B3["여러 번 ON/OFF"]
    end
    
    subgraph "결과"
        R1["한 번 눌렀는데<br/>여러 이벤트 발생!"]
    end
    
    SW --> B1 --> B2 --> B3 --> R1
```

```
실제 버튼 신호:
─────┐   ┌┐┌─┐  ┌────────────────
     └───┘└┘ └──┘
     <──바운싱──>

디바운스 후:
─────┐              ┌────────────────
     └──────────────┘
          깨끗한 신호
```

---

## 🔄 디바운스 상태 머신

```mermaid
stateDiagram-v2
    [*] --> IDLE
    
    IDLE --> WAIT_STABLE : 상태 변화 감지
    
    WAIT_STABLE --> IDLE : 30ms 후 안정<br/>이벤트 발생!
    WAIT_STABLE --> IDLE : 원래 상태로 복귀<br/>(노이즈로 판단)
    
    note right of IDLE : 안정 상태 모니터링
    note right of WAIT_STABLE : 디바운스 시간 대기
```

---

## 📊 디바운스 타이밍

```mermaid
sequenceDiagram
    participant BTN as Button
    participant SCAN as Scan Task
    participant DB as Debounce FSM
    participant Q as Event Queue
    participant H as Handler
    
    Note over BTN: 버튼 눌림
    BTN->>SCAN: 상태 변화 감지
    SCAN->>DB: IDLE → WAIT_STABLE
    Note over DB: 타이머 시작
    
    loop 10ms마다 폴링
        SCAN->>BTN: 상태 확인
        BTN-->>SCAN: 아직 눌림
    end
    
    Note over DB: 30ms 경과
    DB->>DB: 상태 확정!
    DB->>Q: BUTTON_EVENT_PRESSED
    Q-->>H: 이벤트 처리
    
    DB->>DB: WAIT_STABLE → IDLE
```

---

## 📦 이벤트 종류

```mermaid
graph LR
    subgraph "기본 이벤트"
        E1["PRESSED<br/>눌림 확정"]
        E2["RELEASED<br/>떼어짐 확정"]
    end
    
    subgraph "복합 이벤트"
        E3["LONG_PRESS<br/>1초 이상 누름"]
        E4["CLICK<br/>짧게 눌렀다 뗌"]
        E5["DOUBLE_CLICK<br/>빠른 두 번 클릭"]
    end
```

---

## 🏗️ 시스템 아키텍처

```mermaid
graph TB
    subgraph "하드웨어"
        BTN["🔘 Button<br/>GPIO"]
    end
    
    subgraph "Scan Task"
        POLL["10ms 주기 폴링"]
        DB["디바운스 FSM"]
    end
    
    subgraph "Event Queue"
        Q["📬 ButtonEvent_t"]
    end
    
    subgraph "Handler Task"
        H["이벤트 처리<br/>LED 토글 등"]
    end
    
    BTN -->|"GPIO Read"| POLL
    POLL --> DB
    DB -->|"xQueueSend()"| Q
    Q -->|"xQueueReceive()"| H
```

---

## ⚙️ 디바운스 알고리즘

```mermaid
flowchart TD
    A["10ms 주기 호출"] --> B{"현재 상태 읽기"}
    B --> C{"디바운스 상태?"}
    
    C -->|IDLE| D{"상태 변화?"}
    D -->|Yes| E["WAIT_STABLE로 전이<br/>타이머 시작"]
    D -->|No| F{"눌린 상태?"}
    F -->|Yes| G{"1초 경과?"}
    G -->|Yes| H["LONG_PRESS 이벤트"]
    
    C -->|WAIT_STABLE| I{"30ms 경과?"}
    I -->|Yes| J{"상태 확정?"}
    J -->|Yes| K["PRESSED/RELEASED 이벤트"]
    J -->|No| L["노이즈로 무시"]
    I -->|No| M["계속 대기"]
```

---

## 🔍 핵심 코드 분석

### 버튼 상태 구조체

```c
typedef struct
{
    BaseType_t xCurrentState;       // 현재 읽은 상태
    BaseType_t xLastStableState;    // 마지막 안정 상태
    DebounceState_t eDebounceState; // 디바운스 상태
    TickType_t xLastChangeTime;     // 마지막 상태 변화 시간
    TickType_t xPressStartTime;     // 눌림 시작 시간
    BaseType_t xLongPressReported;  // 롱 프레스 이미 보고됨
} ButtonState_t;
```

### 디바운스 로직

```c
void prvProcessButtonDebounce(void)
{
    TickType_t xCurrentTime = xTaskGetTickCount();
    BaseType_t xRawState = prvReadButtonRaw();
    
    switch(g_xButtonState.eDebounceState)
    {
        case DEBOUNCE_IDLE:
            if(xRawState != g_xButtonState.xLastStableState)
            {
                // 상태 변화 감지 → 디바운스 대기 시작
                g_xButtonState.xLastChangeTime = xCurrentTime;
                g_xButtonState.eDebounceState = DEBOUNCE_WAIT_STABLE;
            }
            else if(g_xButtonState.xLastStableState == pdTRUE)
            {
                // 롱 프레스 체크
                if((xCurrentTime - g_xButtonState.xPressStartTime) >= 
                   pdMS_TO_TICKS(LONG_PRESS_TIME_MS))
                {
                    // LONG_PRESS 이벤트!
                }
            }
            break;
            
        case DEBOUNCE_WAIT_STABLE:
            if((xCurrentTime - g_xButtonState.xLastChangeTime) >= 
               pdMS_TO_TICKS(DEBOUNCE_TIME_MS))
            {
                // 디바운스 시간 경과 → 상태 확정
                if(xRawState != g_xButtonState.xLastStableState)
                {
                    g_xButtonState.xLastStableState = xRawState;
                    // PRESSED 또는 RELEASED 이벤트!
                }
                g_xButtonState.eDebounceState = DEBOUNCE_IDLE;
            }
            break;
    }
}
```

---

## 📋 설계 파라미터

| 파라미터 | 값 | 설명 |
|----------|-----|------|
| DEBOUNCE_TIME_MS | 30ms | 디바운스 대기 시간 |
| LONG_PRESS_TIME_MS | 1000ms | 롱 프레스 판정 시간 |
| SCAN_PERIOD_MS | 10ms | 버튼 폴링 주기 |

> [!TIP]
> 폴링 주기는 디바운스 시간의 1/3 이하로 설정

---

## 🎯 이벤트 처리 예시

```mermaid
graph LR
    subgraph "이벤트"
        E1["CLICK"]
        E2["LONG_PRESS"]
        E3["DOUBLE_CLICK"]
    end
    
    subgraph "동작"
        A1["LED 토글"]
        A2["설정 모드 진입"]
        A3["값 초기화"]
    end
    
    E1 --> A1
    E2 --> A2
    E3 --> A3
```

---

## 💡 핵심 정리

1. **바운싱**: 기계식 스위치의 접점 진동
2. **디바운스**: 상태 안정까지 30ms 대기
3. **상태 머신**: IDLE ↔ WAIT_STABLE
4. **분리**: 폴링(Scan)과 처리(Handler) 분리
