# Example 08: Software Timer 데모

## 📌 학습 목표

- Software Timer의 One-Shot / Auto-Reload 모드
- Timer Service Task (Daemon) 동작 이해
- 태스크 없이 주기적 콜백 실행

---

## 🏗️ Timer 아키텍처

```mermaid
graph TB
    subgraph "Application Tasks"
        T1["Task A"]
        T2["Task B"]
    end
    
    subgraph "Timer Command Queue"
        TQ["📬 Timer Queue<br/>명령 대기열"]
    end
    
    subgraph "Timer Service Task"
        TS["⏰ Timer Daemon<br/>prvTimerTask()"]
    end
    
    subgraph "Timer List"
        TL["📋 Active Timer List<br/>만료 시간순 정렬"]
    end
    
    subgraph "Callbacks"
        CB1["Callback 1"]
        CB2["Callback 2"]
    end
    
    T1 -->|"xTimerStart()"| TQ
    T2 -->|"xTimerStop()"| TQ
    TQ --> TS
    TS --> TL
    TL -->|"만료!"| CB1
    TL -->|"만료!"| CB2
```

---

## 🔄 One-Shot vs Auto-Reload

```mermaid
gantt
    title Timer 타입 비교
    dateFormat X
    axisFormat %L
    
    section One-Shot
    대기     :os1, 0, 500
    콜백     :crit, os2, 500, 510
    정지     :os3, 510, 1000
    
    section Auto-Reload
    대기     :ar1, 0, 200
    콜백     :crit, ar2, 200, 210
    대기     :ar3, 210, 410
    콜백     :crit, ar4, 410, 420
    대기     :ar5, 420, 620
    콜백     :crit, ar6, 620, 630
```

```mermaid
graph TB
    subgraph "One-Shot (pdFALSE)"
        OS1["xTimerStart()"] --> OS2["Period 대기"]
        OS2 --> OS3["Callback 실행"]
        OS3 --> OS4["Dormant 상태"]
    end
    
    subgraph "Auto-Reload (pdTRUE)"
        AR1["xTimerStart()"] --> AR2["Period 대기"]
        AR2 --> AR3["Callback 실행"]
        AR3 --> AR2
    end
```

---

## 📦 Timer 내부 구조

```mermaid
classDiagram
    class Timer_t {
        +const char *pcTimerName
        +ListItem_t xTimerListItem
        +TickType_t xTimerPeriodInTicks
        +BaseType_t xAutoReload
        +void *pvTimerID
        +TimerCallbackFunction_t pxCallbackFunction
    }
    
    class TimerCommandQueue {
        +QueueHandle_t xTimerQueue
        +depth: 5~10
    }
    
    class ActiveTimerList {
        +List_t xActiveTimerList1
        +List_t xActiveTimerList2
    }
    
    Timer_t --> TimerCommandQueue : 명령 전송
    TimerCommandQueue --> ActiveTimerList : Daemon이 처리
```

---

## ⚙️ 커널 동작 원리

### Timer Service Task (Daemon)

```mermaid
sequenceDiagram
    participant App as Application
    participant TQ as Timer Queue
    participant TS as Timer Service Task
    participant TL as Active Timer List
    
    Note over TS: prvTimerTask() 무한 루프
    
    TS->>TQ: xQueueReceive(timeout)
    Note over TS: 명령 대기 또는<br/>다음 만료 시간까지
    
    alt 명령 수신
        TQ-->>TS: START/STOP/RESET 명령
        TS->>TL: 타이머 추가/제거
    else 타임아웃 (타이머 만료)
        TS->>TL: 만료된 타이머 확인
        TS->>TS: pxCallbackFunction() 호출
        alt Auto-Reload
            TS->>TL: 다시 삽입
        else One-Shot
            TS->>TS: Dormant 상태로 변경
        end
    end
```

### xTimerStart() 내부

```mermaid
flowchart TD
    A["xTimerStart(xTimer, xTicksToWait)"] --> B["명령 메시지 생성"]
    B --> C["xQueueSend(xTimerQueue)"]
    C --> D{Timer Daemon 실행 중?}
    D -->|Yes| E["명령 처리"]
    E --> F["xTimerListItem 삽입"]
    F --> G["만료 시간순 정렬"]
    D -->|No| H["Daemon 깨움"]
    H --> E
```

---

## 🔍 커널 코드 분석

### Timer 생성 (timers.c)

```c
TimerHandle_t xTimerCreate(const char *pcTimerName,
                           const TickType_t xTimerPeriodInTicks,
                           const BaseType_t xAutoReload,
                           void *pvTimerID,
                           TimerCallbackFunction_t pxCallbackFunction)
{
    Timer_t *pxNewTimer;
    
    pxNewTimer = pvPortMalloc(sizeof(Timer_t));
    
    if(pxNewTimer != NULL)
    {
        pxNewTimer->pcTimerName = pcTimerName;
        pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;
        pxNewTimer->xAutoReload = xAutoReload;
        pxNewTimer->pvTimerID = pvTimerID;
        pxNewTimer->pxCallbackFunction = pxCallbackFunction;
        
        vListInitialiseItem(&(pxNewTimer->xTimerListItem));
        listSET_LIST_ITEM_OWNER(&(pxNewTimer->xTimerListItem), pxNewTimer);
    }
    
    return pxNewTimer;
}
```

### Timer Daemon 루프 (timers.c)

```c
static portTASK_FUNCTION(prvTimerTask, pvParameters)
{
    TickType_t xNextExpireTime;
    BaseType_t xListWasEmpty;
    
    for(;;)
    {
        // 다음 만료 시간 계산
        xNextExpireTime = prvGetNextExpireTime(&xListWasEmpty);
        
        // 만료된 타이머 처리 또는 명령 대기
        prvProcessTimerOrBlockTask(xNextExpireTime, xListWasEmpty);
        
        // 명령 큐 처리
        prvProcessReceivedCommands();
    }
}
```

### 콜백 실행

```c
static void prvProcessExpiredTimer(TickType_t xNextExpireTime,
                                   TickType_t xTimeNow)
{
    Timer_t *pxTimer;
    
    pxTimer = listGET_OWNER_OF_HEAD_ENTRY(pxCurrentTimerList);
    uxListRemove(&(pxTimer->xTimerListItem));
    
    // 콜백 실행!
    pxTimer->pxCallbackFunction((TimerHandle_t)pxTimer);
    
    if(pxTimer->xAutoReload == pdTRUE)
    {
        // Auto-Reload: 다시 삽입
        prvReloadTimer(pxTimer, xNextExpireTime, xTimeNow);
    }
    // One-Shot: 삽입하지 않음 (Dormant)
}
```

---

## ⚠️ 콜백 제약사항

```mermaid
graph LR
    subgraph "❌ 콜백에서 사용 금지"
        N1["vTaskDelay()"]
        N2["xQueueReceive(timeout>0)"]
        N3["xSemaphoreTake(timeout>0)"]
    end
    
    subgraph "✅ 콜백에서 사용 가능"
        Y1["xQueueSend()"]
        Y2["xSemaphoreGive()"]
        Y3["xTaskNotify()"]
    end
```

> [!CAUTION]
> 콜백은 Timer Service Task에서 실행됩니다.
> 블로킹하면 다른 타이머도 지연됩니다!

---

## 📋 API 요약

| API | 설명 |
|-----|------|
| `xTimerCreate()` | 타이머 생성 |
| `xTimerStart()` | 타이머 시작 |
| `xTimerStop()` | 타이머 정지 |
| `xTimerReset()` | 리셋 및 재시작 |
| `xTimerChangePeriod()` | 주기 변경 |
| `xTimerIsTimerActive()` | 활성 여부 확인 |
| `pvTimerGetTimerID()` | 타이머 ID 조회 |

---

## 🚀 사용 방법

```c
// One-Shot 타이머 (5초 후 한 번)
xOneShotTimer = xTimerCreate("OneShot",
                              pdMS_TO_TICKS(5000),
                              pdFALSE,  // One-Shot
                              NULL,
                              vOneShotCallback);

// Auto-Reload 타이머 (1초마다)
xAutoReloadTimer = xTimerCreate("AutoReload",
                                 pdMS_TO_TICKS(1000),
                                 pdTRUE,  // Auto-Reload
                                 NULL,
                                 vAutoReloadCallback);

// 시작
xTimerStart(xOneShotTimer, 0);
xTimerStart(xAutoReloadTimer, 0);
```

---

## 💡 핵심 정리

1. **Daemon Task**: 모든 콜백은 Timer Service Task에서 실행
2. **One-Shot**: 한 번 실행 후 Dormant
3. **Auto-Reload**: 자동 반복
4. **블로킹 금지**: 콜백에서 대기 API 사용 불가
