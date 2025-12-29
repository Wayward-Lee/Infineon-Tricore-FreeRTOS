# Example 04: Queue 통신 데모 (Producer/Consumer)

## 📌 학습 목표

- FreeRTOS Queue를 사용한 태스크 간 데이터 통신
- Producer-Consumer 패턴 이해
- Queue 블로킹 메커니즘 파악

---

## 🏗️ 시스템 아키텍처

```mermaid
graph LR
    subgraph "Producer Task"
        P["Producer<br/>센서 데이터 생성"]
    end
    
    subgraph "Queue"
        Q["📦 Queue<br/>FIFO 버퍼<br/>최대 5개"]
    end
    
    subgraph "Consumer Task"
        C["Consumer<br/>데이터 처리"]
    end
    
    P -->|"xQueueSend()"| Q
    Q -->|"xQueueReceive()"| C
```

---

## 🔄 Queue 동작 원리

```mermaid
sequenceDiagram
    participant P as Producer
    participant Q as Queue [5]
    participant C as Consumer
    
    Note over Q: 초기: [ ][ ][ ][ ][ ]
    
    P->>Q: xQueueSend(A)
    Note over Q: [A][ ][ ][ ][ ]
    
    P->>Q: xQueueSend(B)
    Note over Q: [A][B][ ][ ][ ]
    
    C->>Q: xQueueReceive()
    Q-->>C: A 반환
    Note over Q: [B][ ][ ][ ][ ]
    
    P->>Q: xQueueSend(C)
    P->>Q: xQueueSend(D)
    P->>Q: xQueueSend(E)
    P->>Q: xQueueSend(F)
    Note over Q: [B][C][D][E][F] (Full!)
    
    P->>Q: xQueueSend(G, timeout)
    Note over P: ⏳ 블로킹 대기...<br/>공간 생길 때까지
    
    C->>Q: xQueueReceive()
    Q-->>C: B 반환
    Note over Q: [C][D][E][F][G] (공간 생김!)
    Note over P: 블로킹 해제, G 삽입 완료
```

---

## 📦 Queue 내부 구조

```mermaid
classDiagram
    class Queue_t {
        +int8_t *pcHead
        +int8_t *pcWriteTo
        +int8_t *pcReadFrom
        +List_t xTasksWaitingToSend
        +List_t xTasksWaitingToReceive
        +UBaseType_t uxMessagesWaiting
        +UBaseType_t uxLength
        +UBaseType_t uxItemSize
    }
    
    class StorageArea {
        Item0
        Item1
        Item2
        ...
        ItemN
    }
    
    Queue_t --> StorageArea : pcHead 포인트
```

```
Queue 메모리 레이아웃:
┌──────────────────────────────────────────────────┐
│ Queue_t 구조체 │ Item[0] │ Item[1] │ ... │ Item[N] │
└──────────────────────────────────────────────────┘
                  ↑         ↑
              pcHead    pcWriteTo
```

---

## ⚙️ 커널 동작 원리

### xQueueCreate() 내부 동작

```mermaid
sequenceDiagram
    participant App as Application
    participant K as Kernel
    participant H as Heap
    
    App->>K: xQueueCreate(5, sizeof(Data))
    K->>K: 총 크기 계산<br/>sizeof(Queue_t) + 5 * sizeof(Data)
    K->>H: pvPortMalloc(총 크기)
    H-->>K: 메모리 블록
    K->>K: prvInitialiseNewQueue()
    Note over K: pcHead/pcWriteTo 설정<br/>리스트 초기화
    K-->>App: QueueHandle_t 반환
```

### xQueueSend() 블로킹 로직

```mermaid
flowchart TD
    A[xQueueSend 호출] --> B{큐에 공간 있음?}
    B -->|Yes| C[데이터 복사]
    C --> D{수신 대기 태스크 있음?}
    D -->|Yes| E[대기 태스크 깨움]
    D -->|No| F[반환]
    E --> F
    
    B -->|No| G{타임아웃 > 0?}
    G -->|No| H[errQUEUE_FULL 반환]
    G -->|Yes| I[xTasksWaitingToSend에 등록]
    I --> J[vTaskPlaceOnEventList]
    J --> K[portYIELD]
    K --> L{깨어남}
    L --> B
```

---

## 🔍 커널 코드 분석

### Queue 생성 (queue.c)

```c
QueueHandle_t xQueueGenericCreate(const UBaseType_t uxQueueLength,
                                   const UBaseType_t uxItemSize,
                                   const uint8_t ucQueueType)
{
    Queue_t *pxNewQueue;
    size_t xQueueSizeInBytes;
    
    // 메모리 크기 계산
    xQueueSizeInBytes = sizeof(Queue_t) + (uxQueueLength * uxItemSize);
    
    pxNewQueue = pvPortMalloc(xQueueSizeInBytes);
    
    if(pxNewQueue != NULL)
    {
        // 스토리지 영역은 Queue_t 바로 뒤
        pxNewQueue->pcHead = ((int8_t*)pxNewQueue) + sizeof(Queue_t);
        
        // 초기화
        prvInitialiseNewQueue(uxQueueLength, uxItemSize, 
                              pxNewQueue->pcHead, ucQueueType, pxNewQueue);
    }
    
    return pxNewQueue;
}
```

### xQueueSend() 핵심 로직 (queue.c)

```c
BaseType_t xQueueGenericSend(QueueHandle_t xQueue,
                             const void *pvItemToQueue,
                             TickType_t xTicksToWait,
                             const BaseType_t xCopyPosition)
{
    Queue_t *pxQueue = xQueue;
    
    for(;;)
    {
        taskENTER_CRITICAL();
        {
            // 공간이 있는지 확인
            if(pxQueue->uxMessagesWaiting < pxQueue->uxLength)
            {
                // 데이터 복사
                prvCopyDataToQueue(pxQueue, pvItemToQueue, xCopyPosition);
                
                // 수신 대기 중인 태스크가 있으면 깨움
                if(!listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)))
                {
                    xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive));
                }
                
                taskEXIT_CRITICAL();
                return pdPASS;
            }
            else
            {
                // 공간 없음 - 대기 필요
                if(xTicksToWait == 0)
                {
                    taskEXIT_CRITICAL();
                    return errQUEUE_FULL;
                }
                
                // 대기 리스트에 등록
                vTaskPlaceOnEventList(&(pxQueue->xTasksWaitingToSend), 
                                      xTicksToWait);
            }
        }
        taskEXIT_CRITICAL();
        
        portYIELD_WITHIN_API();  // 블로킹
    }
}
```

---

## 📋 API 요약

| API | 설명 | 블로킹 |
|-----|------|--------|
| `xQueueCreate()` | 큐 생성 | - |
| `xQueueSend()` | 뒤에 삽입 | ✅ |
| `xQueueSendToFront()` | 앞에 삽입 | ✅ |
| `xQueueReceive()` | 앞에서 제거 | ✅ |
| `xQueuePeek()` | 앞 확인 (제거 안함) | ✅ |
| `uxQueueMessagesWaiting()` | 대기 메시지 수 | ❌ |
| `uxQueueSpacesAvailable()` | 남은 공간 수 | ❌ |

---

## 🔄 값 복사 vs 포인터 전달

```mermaid
graph LR
    subgraph "값 복사 (권장)"
        V1["Producer"] -->|"복사"| V2["Queue"]
        V2 -->|"복사"| V3["Consumer"]
    end
    
    subgraph "포인터 전달 (주의!)"
        P1["Producer"] -->|"포인터만"| P2["Queue"]
        P2 -->|"포인터만"| P3["Consumer"]
        P1 -.->|"원본"| MEM["메모리"]
        P3 -.->|"접근"| MEM
    end
```

> [!WARNING]
> 포인터 전달 시 원본 메모리 수명 관리 필수!

---

## 🚀 사용 방법

```c
// 큐 생성
QueueHandle_t xQueue = xQueueCreate(5, sizeof(SensorData_t));

// Producer
xQueueSend(xQueue, &data, portMAX_DELAY);

// Consumer
xQueueReceive(xQueue, &receivedData, portMAX_DELAY);
```

---

## 💡 핵심 정리

1. **FIFO 버퍼**: 먼저 넣은 데이터가 먼저 나옴
2. **값 복사**: 데이터를 큐 내부로 복사 (안전)
3. **블로킹 지원**: 가득 참/비어있음 시 자동 대기
4. **ISR 안전**: FromISR 버전 API 사용
