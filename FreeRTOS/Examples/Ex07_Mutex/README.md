# Example 07: Mutex 데모 (Priority Inheritance)

## 📌 학습 목표

- Mutex로 공유 자원 보호
- Priority Inheritance 메커니즘 이해
- Mutex와 Binary Semaphore 차이점

---

## ⚠️ Priority Inversion 문제

```mermaid
sequenceDiagram
    participant L as Low (P1)
    participant M as Medium (P2)
    participant H as High (P3)
    participant MUT as Mutex
    
    Note over L,MUT: Priority Inversion 발생!
    
    L->>MUT: Lock (획득)
    Note over L: Mutex 보유 중
    
    H->>MUT: Lock 시도
    Note over H: ⏳ 블로킹 (Mutex 사용 중)
    
    Note over M: Medium 실행 시작!
    M->>M: 작업 수행...
    Note over M: Medium이 Low를 선점
    
    Note over H: ❌ High가 Medium보다<br/>더 오래 대기!
```

---

## ✅ Priority Inheritance 해결책

```mermaid
sequenceDiagram
    participant L as Low (P1→P3)
    participant M as Medium (P2)
    participant H as High (P3)
    participant MUT as Mutex
    
    Note over L,MUT: Priority Inheritance 동작
    
    L->>MUT: Lock (획득)
    Note over L: Mutex 보유 중
    
    H->>MUT: Lock 시도
    Note over H: 블로킹
    Note over L: 📈 우선순위 상승!<br/>P1 → P3
    
    Note over M: Medium 실행 시도
    Note over M: ❌ Low(P3) > Medium(P2)<br/>선점 불가!
    
    Note over L: 작업 완료
    L->>MUT: Unlock
    Note over L: 📉 우선순위 복원<br/>P3 → P1
    
    MUT-->>H: High 바로 실행!
```

---

## 🔄 Priority Inheritance 타임라인

```mermaid
gantt
    title Priority Inheritance 동작
    dateFormat X
    axisFormat %L
    
    section Without Inheritance
    Low 실행     :l1, 0, 10
    Medium 선점  :crit, m1, 10, 50
    Low 재개     :l2, 50, 60
    High 실행    :h1, 60, 70
    
    section With Inheritance
    Low 실행     :l3, 0, 20
    High 실행    :h2, 20, 30
    Medium 실행  :m2, 30, 50
```

---

## 📦 Mutex 내부 구조

```mermaid
classDiagram
    class Queue_t {
        +List_t xTasksWaitingToReceive
        +SemaphoreData_t xSemaphore
        +UBaseType_t uxMessagesWaiting
    }
    
    class SemaphoreData_t {
        +TaskHandle_t xMutexHolder
        +UBaseType_t uxRecursiveCallCount
    }
    
    class TCB_t {
        +UBaseType_t uxPriority
        +UBaseType_t uxBasePriority
        +UBaseType_t uxMutexesHeld
    }
    
    Queue_t --> SemaphoreData_t
    SemaphoreData_t --> TCB_t : xMutexHolder
```

---

## ⚙️ 커널 동작 원리

### Priority Inheritance 구현

```mermaid
sequenceDiagram
    participant H as High Task
    participant K as Kernel
    participant MUT as Mutex
    participant L as Low Task (Holder)
    
    H->>K: xSemaphoreTake(mutex)
    K->>MUT: Mutex 보유자 확인
    MUT-->>K: xMutexHolder = Low Task
    K->>K: Low->uxPriority < High->uxPriority?
    Note over K: Yes! 상속 필요
    K->>L: uxPriority = High->uxPriority
    Note over L: 우선순위 상승!
    K->>K: Low를 Ready List에서<br/>새 우선순위 위치로 이동
    K->>H: 대기 리스트에 등록
```

### xSemaphoreGive() 우선순위 복원

```mermaid
flowchart TD
    A[xSemaphoreGive 호출] --> B{uxMutexesHeld > 0?}
    B -->|Yes| C["uxMutexesHeld--"]
    C --> D{uxMutexesHeld == 0?}
    D -->|Yes| E["uxPriority = uxBasePriority"]
    E --> F[Ready List 재배치]
    D -->|No| G[우선순위 유지]
    B -->|No| H[에러]
```

---

## 🔍 커널 코드 분석

### Mutex 생성 (semphr.h → queue.c)

```c
#define xSemaphoreCreateMutex() \
    xQueueCreateMutex(queueQUEUE_TYPE_MUTEX)

QueueHandle_t xQueueCreateMutex(const uint8_t ucQueueType)
{
    QueueHandle_t xNewQueue;
    
    xNewQueue = xQueueGenericCreate(1, 0, ucQueueType);
    
    if(xNewQueue != NULL)
    {
        // Mutex는 초기 상태가 Available (1)
        ((Queue_t*)xNewQueue)->uxMessagesWaiting = 1;
        
        // 소유자 없음
        ((Queue_t*)xNewQueue)->u.xSemaphore.xMutexHolder = NULL;
    }
    
    return xNewQueue;
}
```

### Priority Inheritance 로직 (tasks.c)

```c
BaseType_t xTaskPriorityInherit(TaskHandle_t const pxMutexHolder)
{
    TCB_t *pxMutexHolderTCB = pxMutexHolder;
    
    if(pxMutexHolderTCB->uxPriority < pxCurrentTCB->uxPriority)
    {
        // Ready List에서 제거
        uxListRemove(&(pxMutexHolderTCB->xStateListItem));
        
        // 우선순위 상승
        pxMutexHolderTCB->uxPriority = pxCurrentTCB->uxPriority;
        
        // 새 우선순위의 Ready List에 추가
        prvAddTaskToReadyList(pxMutexHolderTCB);
        
        return pdTRUE;
    }
    
    return pdFALSE;
}
```

### Priority Disinherit (락 해제 시)

```c
BaseType_t xTaskPriorityDisinherit(TaskHandle_t const pxMutexHolder)
{
    TCB_t *pxTCB = pxMutexHolder;
    
    pxTCB->uxMutexesHeld--;
    
    if(pxTCB->uxPriority != pxTCB->uxBasePriority)
    {
        if(pxTCB->uxMutexesHeld == 0)
        {
            // 원래 우선순위로 복원
            uxListRemove(&(pxTCB->xStateListItem));
            pxTCB->uxPriority = pxTCB->uxBasePriority;
            prvAddTaskToReadyList(pxTCB);
            
            return pdTRUE;
        }
    }
    
    return pdFALSE;
}
```

---

## 🆚 Mutex vs Binary Semaphore

```mermaid
graph TB
    subgraph "Mutex"
        M1["소유권 있음"]
        M2["Priority Inheritance"]
        M3["Take한 태스크만 Give 가능"]
        M4["초기: Available (1)"]
        M5["Recursive Mutex 지원"]
    end
    
    subgraph "Binary Semaphore"
        B1["소유권 없음"]
        B2["Priority Inheritance 없음"]
        B3["아무 태스크나 Give 가능"]
        B4["초기: Empty (0)"]
        B5["Recursive 없음"]
    end
```

---

## 🔄 Recursive Mutex

```mermaid
sequenceDiagram
    participant T as Task
    participant RM as Recursive Mutex
    
    T->>RM: Take (count=1)
    Note over T: 재귀 함수 호출
    T->>RM: Take (count=2)
    Note over T: 같은 태스크이므로 OK!
    T->>T: 작업 수행
    T->>RM: Give (count=1)
    T->>RM: Give (count=0)
    Note over RM: 이제 다른 태스크 획득 가능
```

---

## 🚀 사용 방법

```c
// Mutex 생성
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();

// Critical Section 보호
void vTask(void *pvParameters)
{
    for(;;)
    {
        if(xSemaphoreTake(xMutex, portMAX_DELAY) == pdPASS)
        {
            // 공유 자원 접근
            g_sharedData++;
            
            // 반드시 해제!
            xSemaphoreGive(xMutex);
        }
    }
}
```

---

## ⚠️ 데드락 주의

```mermaid
graph LR
    subgraph "데드락 발생"
        T1["Task A"] -->|"Lock M1"| M1["Mutex 1"]
        T2["Task B"] -->|"Lock M2"| M2["Mutex 2"]
        T1 -.->|"대기 M2"| M2
        T2 -.->|"대기 M1"| M1
    end
```

> [!CAUTION]
> 여러 Mutex 사용 시 **항상 같은 순서**로 획득!

---

## 💡 핵심 정리

1. **소유권**: Take한 태스크만 Give 가능
2. **Priority Inheritance**: 우선순위 역전 방지
3. **초기 Available**: 생성 직후 Take 가능
4. **데드락 주의**: 다중 Mutex 획득 순서 일관성
