# Example 11: LED 패턴 컨트롤러 데모

## 📌 학습 목표

- Queue + Timer + State Machine 조합
- 명령-처리자 아키텍처 패턴
- 실제 임베디드 시스템 설계 적용

---

## 🏗️ 시스템 아키텍처

```mermaid
graph TB
    subgraph "명령 생성"
        CMD["Commander Task<br/>패턴 명령 생성"]
    end
    
    subgraph "Queue"
        Q["📬 Command Queue<br/>PatternCommand_t"]
    end
    
    subgraph "패턴 처리"
        DRV["Pattern Driver Task<br/>명령 해석 및 실행"]
    end
    
    subgraph "타이밍"
        TMR["⏰ Software Timer<br/>패턴 타이밍"]
    end
    
    subgraph "출력"
        LED["💡 LED<br/>P22.5"]
    end
    
    CMD -->|"xQueueSend()"| Q
    Q -->|"xQueueReceive()"| DRV
    DRV -->|"xTimerStart()"| TMR
    TMR -->|"Callback"| LED
```

---

## 🔄 패턴 종류

```mermaid
graph LR
    subgraph "PATTERN_OFF"
        OFF["LED 끄기"]
    end
    
    subgraph "PATTERN_ON"
        ON["LED 켜기"]
    end
    
    subgraph "PATTERN_BLINK_SLOW"
        BS["500ms ON/OFF"]
    end
    
    subgraph "PATTERN_BLINK_FAST"
        BF["100ms ON/OFF"]
    end
    
    subgraph "PATTERN_HEARTBEAT"
        HB["💓 두 번 깜빡 + 긴 대기"]
    end
```

---

## ❤️ Heartbeat 상태 머신

```mermaid
stateDiagram-v2
    [*] --> BEAT1_ON
    
    BEAT1_ON --> BEAT1_OFF : 100ms
    BEAT1_OFF --> BEAT2_ON : 100ms
    BEAT2_ON --> BEAT2_OFF : 100ms
    BEAT2_OFF --> PAUSE : 100ms
    PAUSE --> BEAT1_ON : 600ms
    
    note right of BEAT1_ON : LED ON
    note right of BEAT1_OFF : LED OFF
    note right of BEAT2_ON : LED ON
    note right of BEAT2_OFF : LED OFF
    note right of PAUSE : LED OFF (긴 대기)
```

---

## 📊 타이밍 다이어그램

```mermaid
gantt
    title Heartbeat 패턴 (1초 주기)
    dateFormat X
    axisFormat %L
    
    section LED
    ON      :crit, a1, 0, 100
    OFF     :a2, 100, 200
    ON      :crit, a3, 200, 300
    OFF     :a4, 300, 900
    ON      :crit, a5, 900, 1000
```

---

## 📦 데이터 구조

```mermaid
classDiagram
    class LEDPattern_t {
        <<enumeration>>
        PATTERN_OFF
        PATTERN_ON
        PATTERN_BLINK_SLOW
        PATTERN_BLINK_FAST
        PATTERN_HEARTBEAT
        PATTERN_SOS
    }
    
    class PatternCommand_t {
        +LEDPattern_t ePattern
        +uint32_t ulDuration
    }
    
    class HeartbeatState_t {
        <<enumeration>>
        BEAT1_ON
        BEAT1_OFF
        BEAT2_ON
        BEAT2_OFF
        PAUSE
    }
    
    PatternCommand_t --> LEDPattern_t
```

---

## ⚙️ 동작 시퀀스

```mermaid
sequenceDiagram
    participant C as Commander
    participant Q as Queue
    participant D as Driver
    participant T as Timer
    participant L as LED
    
    C->>Q: xQueueSend(BLINK_FAST)
    Q-->>D: 명령 수신
    D->>T: xTimerChangePeriod(100ms)
    D->>T: xTimerStart()
    
    loop 100ms마다
        T->>T: Callback 실행
        T->>L: 토글
    end
    
    Note over C: 5초 후...
    C->>Q: xQueueSend(HEARTBEAT)
    Q-->>D: 명령 수신
    D->>T: xTimerStop()
    D->>T: 상태 머신 초기화
    D->>T: xTimerStart()
```

---

## 🔍 핵심 코드 분석

### 명령 전송

```c
typedef struct
{
    LEDPattern_t ePattern;
    uint32_t ulDuration;  // 0 = 무한
} PatternCommand_t;

// Commander에서 명령 전송
PatternCommand_t xCommand;
xCommand.ePattern = PATTERN_HEARTBEAT;
xCommand.ulDuration = 0;
xQueueSend(xCommandQueue, &xCommand, 0);
```

### 패턴 처리

```c
// Driver Task
for(;;)
{
    if(xQueueReceive(xCommandQueue, &xCmd, portMAX_DELAY) == pdPASS)
    {
        xTimerStop(xPatternTimer, 0);
        g_eCurrentPattern = xCmd.ePattern;
        
        switch(xCmd.ePattern)
        {
            case PATTERN_BLINK_FAST:
                xTimerChangePeriod(xPatternTimer, pdMS_TO_TICKS(100), 0);
                xTimerStart(xPatternTimer, 0);
                break;
            // ... 다른 패턴들
        }
    }
}
```

### Timer 콜백 (상태 머신)

```c
void prvPatternTimerCallback(TimerHandle_t xTimer)
{
    switch(g_eCurrentPattern)
    {
        case PATTERN_HEARTBEAT:
            switch(g_eHeartbeatState)
            {
                case HEARTBEAT_BEAT1_ON:
                    LED_ON();
                    g_eHeartbeatState = HEARTBEAT_BEAT1_OFF;
                    xTimerChangePeriod(xTimer, pdMS_TO_TICKS(100), 0);
                    break;
                // ... 상태 전이
            }
            break;
    }
}
```

---

## 🎯 설계 패턴

```mermaid
graph TB
    subgraph "Command Pattern"
        CP1["명령 객체화"]
        CP2["큐로 전달"]
        CP3["비동기 실행"]
    end
    
    subgraph "State Machine Pattern"
        SM1["상태 정의"]
        SM2["전이 조건"]
        SM3["상태별 동작"]
    end
    
    subgraph "Observer Pattern (확장)"
        OP1["상태 변화 감지"]
        OP2["리스너 통지"]
    end
```

---

## 🚀 확장 가능성

```mermaid
graph LR
    subgraph "현재"
        C1["1개 LED"]
        C2["5개 패턴"]
    end
    
    subgraph "확장"
        E1["다중 LED"]
        E2["PWM 밝기"]
        E3["RGB 색상"]
        E4["UART 명령"]
        E5["버튼 입력"]
    end
    
    C1 --> E1
    C2 --> E2
    C2 --> E3
    C2 --> E4
    C2 --> E5
```

---

## 💡 핵심 정리

1. **분리**: 명령 생성과 처리 분리 (Loose Coupling)
2. **버퍼링**: Queue로 명령 버퍼링
3. **상태 머신**: 복잡한 패턴을 상태로 관리
4. **Timer**: 정밀한 타이밍 제어
