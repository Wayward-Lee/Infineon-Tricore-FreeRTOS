# Example 01: 다중 태스크 우선순위 데모

## 📌 학습 목표

- FreeRTOS 태스크 우선순위 개념 이해
- 선점형(Preemptive) 스케줄링 동작 원리 파악
- `xTaskCreate()` 함수를 사용한 태스크 생성

---

## 🏗️ 시스템 아키텍처

```mermaid
graph TB
    subgraph "FreeRTOS Scheduler"
        SCH[Scheduler<br/>우선순위 기반 선택]
    end
    
    subgraph "Ready Queue"
        RQ[Ready List Array<br/>pxReadyTasksLists]
    end
    
    subgraph "Tasks"
        HIGH["High Priority Task<br/>Priority 3<br/>100ms LED 토글"]
        MED["Medium Priority Task<br/>Priority 2<br/>300ms 카운트"]
        LOW["Low Priority Task<br/>Priority 1<br/>500ms 카운트"]
    end
    
    HIGH --> RQ
    MED --> RQ
    LOW --> RQ
    RQ --> SCH
    SCH -->|"가장 높은 우선순위 선택"| HIGH
```

---

## 🔧 사용된 FreeRTOS API

| API | 설명 |
|-----|------|
| `xTaskCreate()` | 새로운 태스크 생성 |
| `vTaskDelay()` | 상대적 시간 딜레이 |
| `vTaskPrioritySet()` | 태스크 우선순위 동적 변경 |
| `uxTaskPriorityGet()` | 현재 우선순위 조회 |

---

## ⚙️ 커널 동작 원리

### xTaskCreate() 내부 동작

```mermaid
sequenceDiagram
    participant App as Application
    participant Kernel as FreeRTOS Kernel
    participant Heap as Heap Memory
    participant Ready as Ready List
    
    App->>Kernel: xTaskCreate(pvTaskCode, pcName, usStackDepth, ...)
    Kernel->>Heap: pvPortMalloc(TCB + Stack)
    Heap-->>Kernel: TCB 메모리 할당
    Kernel->>Kernel: pxNewTCB 초기화
    Kernel->>Kernel: pxPortInitialiseStack()
    Note over Kernel: 스택에 초기 컨텍스트 설정<br/>(PC, LR, R0-R12 등)
    Kernel->>Ready: prvAddTaskToReadyList(pxNewTCB)
    Note over Ready: pxReadyTasksLists[priority]에 추가
    Kernel-->>App: pdPASS 반환
```

### 스케줄러 선택 알고리즘

```c
// tasks.c의 핵심 매크로
#define taskSELECT_HIGHEST_PRIORITY_TASK()                          \
{                                                                    \
    UBaseType_t uxTopPriority = uxTopReadyPriority;                 \
    while(listLIST_IS_EMPTY(&pxReadyTasksLists[uxTopPriority]))     \
    {                                                                \
        --uxTopPriority;                                            \
    }                                                                \
    listGET_OWNER_OF_NEXT_ENTRY(pxCurrentTCB,                       \
                                &pxReadyTasksLists[uxTopPriority]); \
}
```

### TCB (Task Control Block) 구조

```mermaid
classDiagram
    class TCB_t {
        +StackType_t *pxTopOfStack
        +ListItem_t xStateListItem
        +ListItem_t xEventListItem
        +UBaseType_t uxPriority
        +StackType_t *pxStack
        +char pcTaskName[16]
        +UBaseType_t uxBasePriority
        +uint32_t ulNotifiedValue
    }
    
    class StateListItem {
        TickType_t xItemValue
        ListItem_t *pxNext
        ListItem_t *pxPrevious
        TCB_t *pvOwner
        List_t *pvContainer
    }
    
    TCB_t --> StateListItem : xStateListItem
```

---

## 📋 예제 구성

```mermaid
graph LR
    subgraph "3개의 태스크"
        H["🔴 HIGH<br/>Priority 3"]
        M["🟡 MEDIUM<br/>Priority 2"]
        L["🟢 LOW<br/>Priority 1"]
    end
    
    subgraph "동작"
        H -->|100ms| LED["💡 LED 토글"]
        M -->|300ms| CNT1["📊 카운트++"]
        L -->|500ms| CNT2["📊 카운트++"]
    end
```

---

## 🔄 선점형 스케줄링 동작

```mermaid
gantt
    title 태스크 실행 타임라인 (1초)
    dateFormat X
    axisFormat %L

    section High (P3)
    실행 :a1, 0, 10
    실행 :a2, 100, 110
    실행 :a3, 200, 210
    실행 :a4, 300, 310

    section Medium (P2)
    실행 :b1, 10, 30
    실행 :b2, 310, 330

    section Low (P1)
    실행 :c1, 30, 50
```

### 선점 발생 시퀀스

```mermaid
sequenceDiagram
    participant L as Low Task
    participant M as Medium Task
    participant H as High Task
    participant K as Kernel
    
    L->>L: 실행 중...
    Note over K: Tick Interrupt 발생!
    K->>K: vTaskSwitchContext()
    K->>K: taskSELECT_HIGHEST_PRIORITY_TASK()
    Note over K: High Task가 Ready 상태!
    K-->>H: 컨텍스트 스위칭
    H->>H: 실행
    H->>K: vTaskDelay(100ms)
    K->>K: High → Blocked
    K-->>M: 다음 우선순위 선택
    M->>M: 실행
```

---

## 🚀 사용 방법

```c
void core0_main(void)
{
    DAVE_Init();
    
    Ex01_RunDemo();  // 예제 시작
    
    vTaskStartScheduler();
    while(1) { }
}
```

---

## 📊 예상 결과

10초 실행 후 각 태스크 실행 횟수:
- **High Priority**: ~100회 (100ms 주기)
- **Medium Priority**: ~33회 (300ms 주기)
- **Low Priority**: ~20회 (500ms 주기)

---

## 💡 핵심 개념

### 우선순위 레벨
```
configMAX_PRIORITIES = 10 (FreeRTOSConfig.h)

Priority 9 (최고) ─┐
Priority 8        │ 높은 숫자 = 높은 우선순위
Priority 7        │
...               │
Priority 1        │
Priority 0 (Idle) ─┘
```

---

## 🔍 커널 코드 분석

### Ready List 구조 (tasks.c)

```c
// 우선순위별 Ready List 배열
PRIVILEGED_DATA static List_t pxReadyTasksLists[configMAX_PRIORITIES];

// 태스크를 Ready List에 추가
#define prvAddTaskToReadyList(pxTCB)                                    \
    taskRECORD_READY_PRIORITY((pxTCB)->uxPriority);                     \
    listINSERT_END(&pxReadyTasksLists[(pxTCB)->uxPriority],            \
                   &((pxTCB)->xStateListItem))
```

### 컨텍스트 스위칭 (port.c)

```c
// TriCore 포트의 컨텍스트 스위칭
void vTaskSwitchContext(void)
{
    if(uxSchedulerSuspended != (UBaseType_t)0U)
    {
        xYieldPending = pdTRUE;
    }
    else
    {
        xYieldPending = pdFALSE;
        taskSELECT_HIGHEST_PRIORITY_TASK();  // 핵심!
    }
}
```

---

## 🔍 디버깅 팁

1. **FreeRTOS+Trace** 사용: 태스크 스케줄링 시각화
2. **전역 카운터** 확인: `g_ulHighPriorityCount` 등의 값 모니터링
3. **오실로스코프**: LED 핀 측정으로 정확한 타이밍 확인
