# Example 02: 태스크 상태 관리 데모

## 📌 학습 목표

- FreeRTOS 태스크의 4가지 상태 이해 (Ready, Running, Blocked, Suspended)
- `vTaskSuspend()`, `vTaskResume()` 사용법
- `vTaskDelete()`로 태스크 삭제
- `eTaskGetState()`로 태스크 상태 조회

---

## 🏗️ 태스크 상태 다이어그램

```mermaid
stateDiagram-v2
    [*] --> Ready : xTaskCreate()
    
    Ready --> Running : 스케줄러 선택
    Running --> Ready : 선점됨 (Preempted)
    Running --> Blocked : vTaskDelay() / Queue / Sema
    Running --> Suspended : vTaskSuspend()
    Running --> [*] : vTaskDelete()
    
    Blocked --> Ready : 타임아웃 / 이벤트
    Blocked --> Suspended : vTaskSuspend()
    
    Suspended --> Ready : vTaskResume()
    
    note right of Running : 현재 CPU에서 실행 중
    note right of Ready : 실행 대기 중
    note right of Blocked : 이벤트/시간 대기
    note right of Suspended : 명시적 정지
```

---

## 🔧 사용된 FreeRTOS API

| API | 설명 |
|-----|------|
| `vTaskSuspend()` | 태스크 일시 정지 |
| `vTaskResume()` | 정지된 태스크 재개 |
| `vTaskDelete()` | 태스크 삭제 |
| `eTaskGetState()` | 현재 상태 조회 |
| `xTaskResumeFromISR()` | ISR에서 태스크 재개 |

---

## ⚙️ 커널 동작 원리

### vTaskSuspend() 내부 동작

```mermaid
sequenceDiagram
    participant App as Controller Task
    participant Kernel as Kernel
    participant Worker as Worker Task
    participant SL as xSuspendedTaskList
    
    App->>Kernel: vTaskSuspend(xWorkerHandle)
    Kernel->>Kernel: taskENTER_CRITICAL()
    Kernel->>Kernel: uxListRemove(xStateListItem)
    Note over Kernel: Ready/Blocked 리스트에서 제거
    Kernel->>SL: vListInsertEnd(xSuspendedTaskList)
    Note over SL: Suspended 리스트에 추가
    Kernel->>Kernel: taskEXIT_CRITICAL()
    Note over Worker: Suspended 상태!<br/>스케줄러가 무시함
```

### vTaskResume() 내부 동작

```mermaid
sequenceDiagram
    participant App as Controller Task
    participant Kernel as Kernel
    participant Worker as Worker Task
    participant RL as pxReadyTasksLists
    
    App->>Kernel: vTaskResume(xWorkerHandle)
    Kernel->>Kernel: taskENTER_CRITICAL()
    Kernel->>Kernel: prvTaskIsTaskSuspended() 확인
    Kernel->>Kernel: uxListRemove(xStateListItem)
    Note over Kernel: Suspended 리스트에서 제거
    Kernel->>RL: prvAddTaskToReadyList()
    Note over RL: Ready 리스트에 추가
    alt Worker 우선순위 > 현재 태스크
        Kernel->>Kernel: taskYIELD_WITHIN_API()
    end
    Kernel->>Kernel: taskEXIT_CRITICAL()
    Note over Worker: Ready 상태로 복귀!
```

---

## 📋 예제 시나리오

```mermaid
sequenceDiagram
    participant C as Controller
    participant W as Worker
    participant LED as LED
    
    Note over C,LED: Phase 1: 정상 동작 (3초)
    loop 200ms마다
        W->>LED: 토글
        W->>W: vTaskDelay(200ms)
    end
    
    Note over C,LED: Phase 2: Suspend (2초)
    C->>W: vTaskSuspend()
    Note over W: LED 멈춤!
    C->>C: vTaskDelay(2초)
    
    Note over C,LED: Phase 3: Resume (3초)
    C->>W: vTaskResume()
    loop 200ms마다
        W->>LED: 토글 재개
    end
```

---

## 🔄 Blocked vs Suspended 차이

```mermaid
graph TB
    subgraph "Blocked 상태"
        B1["vTaskDelay() / xQueueReceive()"]
        B2["딜레이/이벤트 리스트에 등록"]
        B3["타임아웃/이벤트 시 자동 Ready"]
    end
    
    subgraph "Suspended 상태"
        S1["vTaskSuspend()"]
        S2["xSuspendedTaskList에 등록"]
        S3["vTaskResume() 필수"]
    end
    
    B1 --> B2 --> B3
    S1 --> S2 --> S3
```

| 특성 | Blocked | Suspended |
|------|---------|-----------|
| 원인 | API 호출 (Delay, Queue 등) | vTaskSuspend() |
| 복귀 | 자동 (타임아웃/이벤트) | vTaskResume() 필수 |
| 저장 위치 | Delayed/Event 리스트 | xSuspendedTaskList |

---

## 🔍 커널 코드 분석

### 태스크 상태 열거형 (task.h)

```c
typedef enum
{
    eRunning = 0,   // 현재 실행 중
    eReady,         // 실행 대기
    eBlocked,       // 이벤트/시간 대기
    eSuspended,     // 명시적 정지
    eDeleted        // 삭제됨 (메모리 해제 대기)
} eTaskState;
```

### vTaskSuspend() 구현 (tasks.c)

```c
void vTaskSuspend(TaskHandle_t xTaskToSuspend)
{
    TCB_t *pxTCB;
    
    taskENTER_CRITICAL();
    {
        pxTCB = prvGetTCBFromHandle(xTaskToSuspend);
        
        // 현재 리스트에서 제거
        if(uxListRemove(&(pxTCB->xStateListItem)) == 0)
        {
            taskRESET_READY_PRIORITY(pxTCB->uxPriority);
        }
        
        // Event 리스트에서도 제거 (있다면)
        if(listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) != NULL)
        {
            uxListRemove(&(pxTCB->xEventListItem));
        }
        
        // Suspended 리스트에 추가
        vListInsertEnd(&xSuspendedTaskList, &(pxTCB->xStateListItem));
    }
    taskEXIT_CRITICAL();
    
    // 자기 자신을 Suspend 했다면 스케줄링
    if(pxTCB == pxCurrentTCB)
    {
        portYIELD_WITHIN_API();
    }
}
```

---

## 🚀 사용 방법

```c
void core0_main(void)
{
    DAVE_Init();
    
    Ex02_RunDemo();  // 예제 시작
    
    vTaskStartScheduler();
    while(1) { }
}
```

---

## ⚠️ 주의사항

```mermaid
graph LR
    subgraph "올바른 사용"
        A1["vTaskSuspend(handle)"] --> A2["외부 태스크 정지"]
        A3["vTaskSuspend(NULL)"] --> A4["자기 자신 정지<br/>(다른 곳에서 Resume 필요!)"]
    end
    
    subgraph "위험한 코드"
        B1["vTaskSuspend(NULL)<br/>+ Resume 없음"] --> B2["❌ 영원히 정지!"]
    end
```

---

## 💡 핵심 정리

1. **Suspend**는 명시적 정지 - Resume 없이는 절대 깨어나지 않음
2. **Blocked**는 조건부 정지 - 조건 충족 시 자동 Ready
3. **Delete**된 태스크의 메모리는 Idle 태스크가 정리
4. ISR에서는 `xTaskResumeFromISR()` 사용 필수
