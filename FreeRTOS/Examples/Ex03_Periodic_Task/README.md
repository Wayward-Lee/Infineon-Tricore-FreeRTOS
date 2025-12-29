# Example 03: 주기적 태스크 실행 데모

## 📌 학습 목표

- `vTaskDelay()`와 `vTaskDelayUntil()` 차이점 이해
- 정밀한 주기적 태스크 구현 방법
- `xTaskGetTickCount()`로 시간 측정

---

## 🔄 vTaskDelay vs vTaskDelayUntil

```mermaid
graph TB
    subgraph "vTaskDelay - 상대적 딜레이"
        A1["실행 시작"] --> A2["작업 수행<br/>(10ms 소요)"]
        A2 --> A3["vTaskDelay(100ms)"]
        A3 --> A4["100ms 대기"]
        A4 --> A5["다시 실행"]
        A5 --> A6["총 주기 = 110ms ❌"]
    end
    
    subgraph "vTaskDelayUntil - 절대적 딜레이"
        B1["실행 시작<br/>T=0"] --> B2["작업 수행<br/>(10ms 소요)"]
        B2 --> B3["vTaskDelayUntil(100ms)"]
        B3 --> B4["90ms만 대기<br/>(자동 보정)"]
        B4 --> B5["다시 실행<br/>T=100"]
        B5 --> B6["총 주기 = 100ms ✅"]
    end
```

---

## 📊 타이밍 비교

```mermaid
gantt
    title 주기 비교 (목표: 100ms)
    dateFormat X
    axisFormat %L
    
    section vTaskDelay
    작업(10ms)     :a1, 0, 10
    대기(100ms)    :a2, 10, 110
    작업(15ms)     :a3, 110, 125
    대기(100ms)    :a4, 125, 225
    
    section vTaskDelayUntil
    작업(10ms)     :b1, 0, 10
    대기(90ms)     :b2, 10, 100
    작업(15ms)     :b3, 100, 115
    대기(85ms)     :b4, 115, 200
```

---

## ⚙️ 커널 동작 원리

### vTaskDelay() 내부 동작

```mermaid
sequenceDiagram
    participant Task as Task
    participant Kernel as Kernel
    participant DL as xDelayedTaskList
    
    Task->>Kernel: vTaskDelay(100 ticks)
    Kernel->>Kernel: xTickCount = 현재 틱
    Kernel->>Kernel: xTimeToWake = xTickCount + 100
    Kernel->>DL: vListInsert(xTimeToWake 기준 정렬)
    Note over DL: Delayed 리스트에 삽입
    Kernel->>Kernel: portYIELD_WITHIN_API()
    Note over Task: Blocked 상태!
    
    Note over Kernel: ... 100 ticks 후 ...
    Kernel->>Kernel: vTaskIncrementTick()
    Kernel->>Kernel: xTimeToWake 확인
    Kernel->>Task: Ready 리스트로 이동
```

### vTaskDelayUntil() 핵심 로직

```mermaid
sequenceDiagram
    participant Task as Task
    participant Kernel as Kernel
    participant LWT as pxPreviousWakeTime
    
    Note over LWT: 마지막 깨어난 시간 저장
    
    Task->>Kernel: vTaskDelayUntil(&xLastWakeTime, 100)
    Kernel->>Kernel: xTimeToWake = *pxPreviousWakeTime + xTimeIncrement
    Note over Kernel: 마지막 깨어난 시간 기준!<br/>실행 시간 무관
    Kernel->>Kernel: 실제 대기 시간 계산
    Note over Kernel: 대기 = xTimeToWake - xTickCount
    Kernel->>Kernel: *pxPreviousWakeTime = xTimeToWake
    Note over LWT: 다음 깨어날 시간 업데이트
    Kernel->>Kernel: Delayed 리스트에 삽입
```

---

## 🔍 커널 코드 분석

### vTaskDelay() 구현 (tasks.c)

```c
void vTaskDelay(const TickType_t xTicksToDelay)
{
    TickType_t xTimeToWake;
    
    if(xTicksToDelay > 0)
    {
        taskENTER_CRITICAL();
        {
            // 현재 시간 + 딜레이 = 깨어날 시간
            xTimeToWake = xTickCount + xTicksToDelay;
            
            // Ready 리스트에서 제거
            uxListRemove(&(pxCurrentTCB->xStateListItem));
            
            // Delayed 리스트에 삽입
            prvAddCurrentTaskToDelayedList(xTimeToWake, pdFALSE);
        }
        taskEXIT_CRITICAL();
        
        portYIELD_WITHIN_API();
    }
}
```

### vTaskDelayUntil() 구현 (tasks.c)

```c
BaseType_t xTaskDelayUntil(TickType_t *pxPreviousWakeTime,
                           const TickType_t xTimeIncrement)
{
    TickType_t xTimeToWake;
    BaseType_t xShouldDelay = pdFALSE;
    
    taskENTER_CRITICAL();
    {
        const TickType_t xConstTickCount = xTickCount;
        
        // 핵심: 마지막 깨어난 시간 + 주기
        xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;
        
        // 아직 깨어날 시간이 안됐으면 대기
        if(xConstTickCount < xTimeToWake)
        {
            prvAddCurrentTaskToDelayedList(xTimeToWake, pdFALSE);
            xShouldDelay = pdTRUE;
        }
        
        // 다음 깨어날 시간 업데이트
        *pxPreviousWakeTime = xTimeToWake;
    }
    taskEXIT_CRITICAL();
    
    if(xShouldDelay)
    {
        portYIELD_WITHIN_API();
    }
    
    return xShouldDelay;
}
```

---

## 📋 Delayed Task List 구조

```mermaid
graph LR
    subgraph "xDelayedTaskList (시간순 정렬)"
        DL["pxDelayedTaskList"]
        DL --> T1["Task A<br/>WakeTime: 150"]
        T1 --> T2["Task B<br/>WakeTime: 200"]
        T2 --> T3["Task C<br/>WakeTime: 350"]
    end
    
    subgraph "Tick ISR"
        TI["vTaskIncrementTick()"]
        TI -->|"xTickCount >= WakeTime?"| CHECK["Head 확인"]
        CHECK -->|"Yes"| MOVE["Ready로 이동"]
    end
```

---

## 🚀 사용 방법

```c
// 정확한 100ms 주기 태스크
void vPeriodicTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();  // 초기화 필수!
    
    for(;;)
    {
        // 작업 수행
        doWork();
        
        // 정확한 100ms 주기 보장
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
```

---

## 📊 적용 시나리오

```mermaid
graph TB
    subgraph "vTaskDelay 사용"
        D1["LED 깜빡임"]
        D2["단발성 대기"]
        D3["대략적 타이밍"]
    end
    
    subgraph "vTaskDelayUntil 사용"
        U1["모터 제어 루프"]
        U2["센서 샘플링"]
        U3["PWM 생성"]
        U4["통신 타이밍"]
    end
```

---

## ⚠️ 주의사항

> [!CAUTION]
> `vTaskDelayUntil()` 사용 시 `xLastWakeTime`을 **반드시 초기화**해야 합니다!

```c
// ❌ 잘못된 코드
void vTask(void)
{
    TickType_t xLastWakeTime;  // 초기화 안됨!
    vTaskDelayUntil(&xLastWakeTime, 100);  // 문제 발생
}

// ✅ 올바른 코드
void vTask(void)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();  // 초기화!
    vTaskDelayUntil(&xLastWakeTime, 100);
}
```

---

## 💡 핵심 정리

| 항목 | vTaskDelay | vTaskDelayUntil |
|------|------------|-----------------|
| 기준점 | 호출 시점 | 마지막 깨어난 시점 |
| 실행시간 영향 | 있음 (주기 늘어남) | 없음 (보정됨) |
| 주기 정확도 | 흔들림 | 정확함 |
| 초기화 | 불필요 | xLastWakeTime 필수 |
| 용도 | 대략적 대기 | 정밀 주기 태스크 |
