# Example 05: Binary Semaphore 데모

## 📌 학습 목표

- Binary Semaphore를 이용한 이벤트 동기화
- ISR-Task 동기화 패턴 이해
- Semaphore와 Mutex의 차이점

---

## 🏗️ 동기화 패턴

```mermaid
graph LR
    subgraph "이벤트 소스"
        ISR["🔔 ISR / Signal Task"]
    end
    
    subgraph "Binary Semaphore"
        SEM["📍 Semaphore<br/>0 or 1"]
    end
    
    subgraph "이벤트 처리"
        HANDLER["⚡ Handler Task"]
    end
    
    ISR -->|"xSemaphoreGive()"| SEM
    SEM -->|"xSemaphoreTake()"| HANDLER
```

---

## 🔄 Binary Semaphore 동작

```mermaid
sequenceDiagram
    participant S as Signal Task/ISR
    participant SEM as Binary Sema [0]
    participant H as Handler Task
    
    Note over SEM: 초기값: 0 (Empty)
    
    H->>SEM: xSemaphoreTake(MAX_DELAY)
    Note over H: ⏳ 블로킹 대기...
    
    Note over S: 이벤트 발생!
    S->>SEM: xSemaphoreGive()
    Note over SEM: 값: 0 → 1
    
    SEM-->>H: 깨어남!
    Note over SEM: 값: 1 → 0
    
    H->>H: 이벤트 처리
    H->>SEM: xSemaphoreTake(MAX_DELAY)
    Note over H: ⏳ 다시 대기...
    
    Note over S: 빠른 연속 Give
    S->>SEM: xSemaphoreGive()
    Note over SEM: 값: 0 → 1
    S->>SEM: xSemaphoreGive()
    Note over SEM: 값: 여전히 1!<br/>(카운트 안됨)
    
    SEM-->>H: 한 번만 깨어남
```

---

## 📦 Semaphore 내부 구조

```mermaid
classDiagram
    class SemaphoreData_t {
        +TaskHandle_t xMutexHolder
        +UBaseType_t uxRecursiveCallCount
    }
    
    class Queue_t {
        +int8_t *pcHead
        +ListItem_t xTasksWaitingToSend
        +ListItem_t xTasksWaitingToReceive
        +SemaphoreData_t xSemaphore
        +UBaseType_t uxMessagesWaiting
        +UBaseType_t uxLength
        +UBaseType_t uxItemSize
    }
    
    Queue_t --> SemaphoreData_t
    
    note for Queue_t "Binary Semaphore는\nQueue의 특수 형태!\nuxLength=1, uxItemSize=0"
```

---

## ⚙️ 커널 동작 원리

### xSemaphoreCreateBinary() 내부

```mermaid
sequenceDiagram
    participant App as Application
    participant K as Kernel
    participant Q as Queue 구조체
    
    App->>K: xSemaphoreCreateBinary()
    K->>K: xQueueGenericCreate(1, 0, queueQUEUE_TYPE_BINARY_SEMAPHORE)
    Note over K: Length=1, ItemSize=0
    K->>Q: Queue 구조체 생성
    Note over Q: uxMessagesWaiting = 0<br/>(초기: Empty)
    K-->>App: SemaphoreHandle_t
```

### xSemaphoreGive() 로직

```mermaid
flowchart TD
    A[xSemaphoreGive 호출] --> B{uxMessagesWaiting < 1?}
    B -->|Yes: 0임| C["uxMessagesWaiting = 1"]
    C --> D{대기 태스크 있음?}
    D -->|Yes| E[xTaskRemoveFromEventList]
    E --> F[대기 태스크 깨움]
    D -->|No| G[반환]
    F --> G
    
    B -->|No: 이미 1| H["변화 없음 (카운트 안됨)"]
    H --> G
```

---

## 🔍 커널 코드 분석

### Binary Semaphore 생성 (semphr.h)

```c
#define xSemaphoreCreateBinary()                                    \
    xQueueGenericCreate(1, semSEMAPHORE_QUEUE_ITEM_LENGTH,         \
                        queueQUEUE_TYPE_BINARY_SEMAPHORE)

// semSEMAPHORE_QUEUE_ITEM_LENGTH = 0
// → 실제 데이터 저장 없음, 카운트만 사용
```

### xSemaphoreGive() 구현 (semphr.h → queue.c)

```c
#define xSemaphoreGive(xSemaphore)  \
    xQueueGenericSend((xSemaphore), NULL, 0, queueSEND_TO_BACK)

// Binary Semaphore에서:
// - NULL 데이터 전송 (ItemSize=0이므로)
// - uxMessagesWaiting가 1 이하일 때만 성공
// - 이미 1이면 변화 없음 (Full 상태)
```

### xSemaphoreTake() 구현

```c
#define xSemaphoreTake(xSemaphore, xBlockTime)  \
    xQueueSemaphoreTake((xSemaphore), (xBlockTime))

// 내부 동작:
// - uxMessagesWaiting > 0 이면 1 감소하고 반환
// - uxMessagesWaiting == 0 이면 블로킹 대기
```

---

## 🆚 Binary Semaphore vs Mutex

```mermaid
graph TB
    subgraph "Binary Semaphore"
        BS1["용도: 동기화/시그널링"]
        BS2["Give/Take 태스크: 다를 수 있음"]
        BS3["Priority Inheritance: 없음"]
        BS4["초기 상태: Empty (0)"]
    end
    
    subgraph "Mutex"
        M1["용도: 상호 배제 (자원 보호)"]
        M2["Give/Take 태스크: 반드시 같음"]
        M3["Priority Inheritance: 있음"]
        M4["초기 상태: Available (1)"]
    end
```

---

## 🔔 ISR에서 사용

```mermaid
sequenceDiagram
    participant HW as Hardware
    participant ISR as ISR
    participant K as Kernel
    participant H as Handler Task
    
    HW->>ISR: 인터럽트!
    ISR->>K: xSemaphoreGiveFromISR(&xHigherPriorityTaskWoken)
    K-->>ISR: pdPASS
    ISR->>ISR: portYIELD_FROM_ISR(xHigherPriorityTaskWoken)
    Note over ISR: ISR 종료 시<br/>컨텍스트 스위칭
    K-->>H: Handler 즉시 실행
```

### ISR 코드 예시

```c
void vExternalInterruptHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // ISR에서는 FromISR 버전 사용!
    xSemaphoreGiveFromISR(xBinarySema, &xHigherPriorityTaskWoken);
    
    // 컨텍스트 스위칭 요청
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

---

## 📋 API 요약

| API | 컨텍스트 | 설명 |
|-----|----------|------|
| `xSemaphoreCreateBinary()` | Task | Binary Sema 생성 |
| `xSemaphoreGive()` | Task | 시그널 발생 |
| `xSemaphoreTake()` | Task | 시그널 대기 |
| `xSemaphoreGiveFromISR()` | **ISR** | ISR용 Give |
| `xSemaphoreTakeFromISR()` | **ISR** | ISR용 Take |

---

## 💡 핵심 정리

1. **Binary**: 값이 0 또는 1만 가능
2. **동기화 용도**: ISR → Task 시그널 전달
3. **초기 Empty**: 생성 직후 Take하면 블로킹
4. **카운트 안됨**: 연속 Give해도 하나만 기록
