# Example 06: Counting Semaphore 데모

## 📌 학습 목표

- Counting Semaphore로 리소스 풀 관리
- Binary vs Counting Semaphore 차이
- 이벤트 카운팅 패턴 이해

---

## 🏗️ 리소스 풀 패턴

```mermaid
graph TB
    subgraph "Workers (5개)"
        W1["Worker 1"]
        W2["Worker 2"]
        W3["Worker 3"]
        W4["Worker 4"]
        W5["Worker 5"]
    end
    
    subgraph "Counting Semaphore"
        SEM["📊 Count: 3<br/>(최대 3개 동시 사용)"]
    end
    
    subgraph "Resources (3개)"
        R1["리소스 A"]
        R2["리소스 B"]
        R3["리소스 C"]
    end
    
    W1 -->|Take| SEM
    W2 -->|Take| SEM
    W3 -->|Take| SEM
    W4 -.->|"대기..."| SEM
    W5 -.->|"대기..."| SEM
    
    SEM -->|Access| R1
    SEM -->|Access| R2
    SEM -->|Access| R3
```

---

## 🔄 Counting Semaphore 동작

```mermaid
sequenceDiagram
    participant W1 as Worker 1
    participant W2 as Worker 2
    participant W3 as Worker 3
    participant W4 as Worker 4
    participant SEM as Counting Sema [3]
    
    Note over SEM: 초기값: 3 (3개 사용 가능)
    
    W1->>SEM: xSemaphoreTake()
    Note over SEM: 3 → 2
    W1->>W1: 리소스 사용 중...
    
    W2->>SEM: xSemaphoreTake()
    Note over SEM: 2 → 1
    
    W3->>SEM: xSemaphoreTake()
    Note over SEM: 1 → 0
    
    W4->>SEM: xSemaphoreTake(TIMEOUT)
    Note over W4: ⏳ 블로킹 대기...<br/>(리소스 없음)
    
    W1->>SEM: xSemaphoreGive()
    Note over SEM: 0 → 1
    SEM-->>W4: 깨어남!
    Note over SEM: 1 → 0
    Note over W4: 리소스 획득!
```

---

## 🆚 Binary vs Counting

```mermaid
graph TB
    subgraph "Binary Semaphore"
        B1["최대값: 1"]
        B2["용도: 이벤트 시그널"]
        B3["연속 Give → 1개만 기록"]
    end
    
    subgraph "Counting Semaphore"
        C1["최대값: N (설정 가능)"]
        C2["용도: 리소스 풀 / 이벤트 카운팅"]
        C3["연속 Give → 모두 카운트"]
    end
```

| 특성 | Binary | Counting |
|------|--------|----------|
| 최대 값 | 1 | N (설정) |
| 연속 Give | 1개만 기록 | 모두 카운트 |
| 주요 용도 | 이벤트 시그널 | 리소스 풀, 카운팅 |

---

## 📊 두 가지 사용 패턴

### 패턴 1: 리소스 풀

```mermaid
graph LR
    subgraph "생성"
        CREATE["xSemaphoreCreateCounting(3, 3)"]
    end
    
    subgraph "동작"
        TAKE["Take → 리소스 획득"]
        USE["사용"]
        GIVE["Give → 리소스 반환"]
    end
    
    CREATE --> TAKE --> USE --> GIVE
```

- **최대값 = 초기값**: 모든 리소스 사용 가능
- Take = 리소스 획득, Give = 리소스 반환

### 패턴 2: 이벤트 카운팅

```mermaid
graph LR
    subgraph "생성"
        CREATE["xSemaphoreCreateCounting(10, 0)"]
    end
    
    subgraph "동작"
        GIVE["Give → 이벤트 발생"]
        TAKE["Take → 이벤트 처리"]
    end
    
    CREATE --> GIVE --> TAKE
```

- **초기값 = 0**: 이벤트 없음
- Give = 이벤트 발생, Take = 이벤트 처리

---

## ⚙️ 커널 동작 원리

### 생성 시 내부 구조

```mermaid
sequenceDiagram
    participant App as Application
    participant K as Kernel
    participant Q as Queue 구조체
    
    App->>K: xSemaphoreCreateCounting(3, 3)
    K->>K: xQueueCreateCountingSemaphore(3, 3)
    K->>Q: Queue 생성 (Length=3)
    Note over Q: uxMessagesWaiting = 3<br/>uxLength = 3
    K-->>App: SemaphoreHandle_t
```

### Take/Give 카운트 변화

```mermaid
flowchart TD
    subgraph "Take (xSemaphoreTake)"
        T1["uxMessagesWaiting > 0?"]
        T1 -->|Yes| T2["uxMessagesWaiting--"]
        T2 --> T3["pdPASS 반환"]
        T1 -->|No| T4["블로킹 대기"]
    end
    
    subgraph "Give (xSemaphoreGive)"
        G1["uxMessagesWaiting < uxLength?"]
        G1 -->|Yes| G2["uxMessagesWaiting++"]
        G2 --> G3["대기 태스크 깨움"]
        G1 -->|No| G4["errQUEUE_FULL"]
    end
```

---

## 🔍 커널 코드 분석

### Counting Semaphore 생성 (semphr.h → queue.c)

```c
#define xSemaphoreCreateCounting(uxMaxCount, uxInitialCount)       \
    xQueueCreateCountingSemaphore((uxMaxCount), (uxInitialCount))

QueueHandle_t xQueueCreateCountingSemaphore(
    const UBaseType_t uxMaxCount,
    const UBaseType_t uxInitialCount)
{
    QueueHandle_t xHandle;
    
    // Queue 생성 (ItemSize = 0)
    xHandle = xQueueGenericCreate(uxMaxCount, 
                                   0,  // 데이터 없음
                                   queueQUEUE_TYPE_COUNTING_SEMAPHORE);
    
    if(xHandle != NULL)
    {
        // 초기 카운트 설정
        ((Queue_t*)xHandle)->uxMessagesWaiting = uxInitialCount;
    }
    
    return xHandle;
}
```

### 카운트 조회 (semphr.h)

```c
#define uxSemaphoreGetCount(xSemaphore)     \
    uxQueueMessagesWaiting((QueueHandle_t)(xSemaphore))

// 현재 사용 가능한 리소스 수 반환
// 리소스 풀: 초기값 - 사용 중인 수
// 이벤트 카운팅: 아직 처리 안된 이벤트 수
```

---

## 📋 예제 시나리오

```mermaid
gantt
    title 5개 Worker가 3개 리소스 경쟁
    dateFormat X
    axisFormat %L

    section Worker 1
    리소스 사용     :w1, 0, 500
    
    section Worker 2
    리소스 사용     :w2, 100, 600
    
    section Worker 3
    리소스 사용     :w3, 200, 700
    
    section Worker 4
    대기            :crit, wait4, 300, 500
    리소스 사용     :w4, 500, 1000
    
    section Worker 5
    대기            :crit, wait5, 400, 600
    리소스 사용     :w5, 600, 1100
```

---

## 🚀 사용 방법

```c
// 리소스 풀 생성 (최대 3개, 초기 3개 사용 가능)
SemaphoreHandle_t xResourcePool = 
    xSemaphoreCreateCounting(3, 3);

// Worker 태스크
void vWorkerTask(void *pvParameters)
{
    for(;;)
    {
        // 리소스 획득 (없으면 대기)
        if(xSemaphoreTake(xResourcePool, portMAX_DELAY) == pdPASS)
        {
            // 리소스 사용
            useResource();
            
            // 리소스 반환 (필수!)
            xSemaphoreGive(xResourcePool);
        }
    }
}
```

---

## ⚠️ 주의사항

> [!CAUTION]
> Take 후 반드시 Give! 그렇지 않으면 리소스 누수 발생

```c
// ❌ 잘못된 코드 - 예외 시 Give 안됨
xSemaphoreTake(xPool, MAX_DELAY);
result = doWork();  // 여기서 에러 발생하면?
xSemaphoreGive(xPool);  // 실행 안됨!

// ✅ 올바른 코드 - 항상 Give
xSemaphoreTake(xPool, MAX_DELAY);
result = doWork();
xSemaphoreGive(xPool);  // 에러와 관계없이 항상 실행
```

---

## 💡 핵심 정리

1. **카운팅 가능**: 0 ~ 최대값까지 카운트
2. **리소스 풀**: 초기값 = 최대값으로 생성
3. **이벤트 카운팅**: 초기값 = 0으로 생성
4. **반환 필수**: Take 후 Give 누락 주의
