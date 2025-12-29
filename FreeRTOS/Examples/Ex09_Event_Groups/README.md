# Example 09: Event Groups 데모

## 📌 학습 목표

- Event Groups로 다중 이벤트 동기화
- AND/OR 조건 대기 이해
- Rendezvous (Barrier) 패턴 구현

---

## 🏗️ 동기화 패턴

```mermaid
graph TB
    subgraph "이벤트 소스들"
        T1["Init Task A<br/>BIT0 설정"]
        T2["Init Task B<br/>BIT1 설정"]
        T3["Init Task C<br/>BIT2 설정"]
    end
    
    subgraph "Event Group"
        EG["📊 Event Bits<br/>[BIT2][BIT1][BIT0]"]
    end
    
    subgraph "대기 태스크"
        MAIN["Main Task<br/>모든 비트 대기"]
    end
    
    T1 -->|"SetBits(0x01)"| EG
    T2 -->|"SetBits(0x02)"| EG
    T3 -->|"SetBits(0x04)"| EG
    EG -->|"WaitBits(0x07)"| MAIN
```

---

## 🔄 AND vs OR 대기

```mermaid
graph LR
    subgraph "AND 모드 (xWaitForAllBits = pdTRUE)"
        A1["대기 비트: 0x07"]
        A2["조건: BIT0 AND BIT1 AND BIT2"]
        A3["모두 설정되어야 깨어남"]
        A1 --> A2 --> A3
    end
    
    subgraph "OR 모드 (xWaitForAllBits = pdFALSE)"
        O1["대기 비트: 0x07"]
        O2["조건: BIT0 OR BIT1 OR BIT2"]
        O3["하나라도 설정되면 깨어남"]
        O1 --> O2 --> O3
    end
```

---

## 📊 비트 설정 시퀀스

```mermaid
sequenceDiagram
    participant A as Task A
    participant B as Task B
    participant C as Task C
    participant EG as Event Group
    participant M as Main Task
    
    Note over EG: 초기: [0][0][0] = 0x00
    
    M->>EG: xEventGroupWaitBits(0x07, AND)
    Note over M: ⏳ 대기 중...
    
    A->>EG: xEventGroupSetBits(0x01)
    Note over EG: [0][0][1] = 0x01
    Note over M: 아직 대기... (0x07 아님)
    
    B->>EG: xEventGroupSetBits(0x02)
    Note over EG: [0][1][1] = 0x03
    Note over M: 아직 대기... (0x07 아님)
    
    C->>EG: xEventGroupSetBits(0x04)
    Note over EG: [1][1][1] = 0x07
    EG-->>M: 깨어남! 0x07 == 0x07
    
    Note over EG: [0][0][0] = 0x00<br/>(xClearOnExit로 클리어)
```

---

## 📦 Event Group 내부 구조

```mermaid
classDiagram
    class EventGroup_t {
        +EventBits_t uxEventBits
        +List_t xTasksWaitingForBits
    }
    
    class WaitingTask {
        +EventBits_t uxBitsToWaitFor
        +BaseType_t xWaitForAllBits
        +BaseType_t xClearBitsOnExit
    }
    
    EventGroup_t --> WaitingTask : 대기 리스트
```

```
Event Bits 구조:
┌──────────────────────────────────────────────────┐
│ Bit23 │ ... │ Bit7 │ Bit6 │ ... │ Bit2 │ Bit1 │ Bit0 │
└──────────────────────────────────────────────────┘
        ↑ 24비트 사용 가능 (32비트 Tick 시)
        ↑ 8비트 사용 가능 (16비트 Tick 시)
```

---

## ⚙️ 커널 동작 원리

### xEventGroupSetBits() 내부

```mermaid
flowchart TD
    A["xEventGroupSetBits(xEventGroup, uxBitsToSet)"] --> B["uxEventBits |= uxBitsToSet"]
    B --> C["대기 리스트 순회"]
    C --> D{각 대기 태스크}
    D --> E{"uxBitsToWaitFor 충족?"}
    E -->|AND 모드| F["모든 비트 설정됨?"]
    E -->|OR 모드| G["하나라도 설정됨?"]
    F -->|Yes| H["태스크 깨움"]
    G -->|Yes| H
    F -->|No| D
    G -->|No| D
    H --> I{xClearOnExit?}
    I -->|Yes| J["비트 클리어"]
    I -->|No| K["유지"]
```

### xEventGroupWaitBits() 내부

```mermaid
sequenceDiagram
    participant T as Task
    participant K as Kernel
    participant EG as Event Group
    participant WL as Wait List
    
    T->>K: xEventGroupWaitBits(0x07, AND)
    K->>EG: uxEventBits 확인
    
    alt 이미 조건 충족
        EG-->>K: 비트 반환
        K-->>T: 즉시 반환
    else 조건 미충족
        K->>WL: 대기 리스트에 등록
        Note over WL: uxBitsToWaitFor = 0x07<br/>xWaitForAllBits = pdTRUE
        K->>K: portYIELD()
        Note over T: 블로킹...
    end
```

---

## 🔍 커널 코드 분석

### Event Group 생성 (event_groups.c)

```c
EventGroupHandle_t xEventGroupCreate(void)
{
    EventGroup_t *pxEventBits;
    
    pxEventBits = pvPortMalloc(sizeof(EventGroup_t));
    
    if(pxEventBits != NULL)
    {
        pxEventBits->uxEventBits = 0;
        vListInitialise(&(pxEventBits->xTasksWaitingForBits));
    }
    
    return pxEventBits;
}
```

### SetBits 로직 (event_groups.c)

```c
EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup,
                               const EventBits_t uxBitsToSet)
{
    EventGroup_t *pxEventBits = xEventGroup;
    ListItem_t *pxListItem;
    EventBits_t uxBitsToClear = 0;
    
    taskENTER_CRITICAL();
    {
        pxEventBits->uxEventBits |= uxBitsToSet;
        
        // 대기 중인 태스크들 확인
        pxListItem = listGET_HEAD_ENTRY(
            &(pxEventBits->xTasksWaitingForBits));
            
        while(pxListItem != listGET_END_MARKER(...))
        {
            // 조건 충족 여부 확인 및 태스크 깨움
            uxBitsToClear |= prvCheckWaitCondition(pxListItem);
            pxListItem = listGET_NEXT(pxListItem);
        }
        
        // 클리어 요청된 비트 제거
        pxEventBits->uxEventBits &= ~uxBitsToClear;
    }
    taskEXIT_CRITICAL();
    
    return pxEventBits->uxEventBits;
}
```

---

## 🔄 Rendezvous (Barrier) 패턴

```mermaid
sequenceDiagram
    participant T1 as Task 1
    participant T2 as Task 2
    participant T3 as Task 3
    participant EG as Event Group
    
    Note over T1,EG: 모든 태스크가 동시에 진행
    
    T1->>EG: xEventGroupSync(BIT0, 0x07)
    Note over T1: 내 비트 설정 + 모든 비트 대기
    
    T2->>EG: xEventGroupSync(BIT1, 0x07)
    
    T3->>EG: xEventGroupSync(BIT2, 0x07)
    Note over EG: 0x07 도달!
    
    Note over T1,T3: 모두 동시에 깨어남!
```

```c
// Rendezvous 구현
EventBits_t uxReturn = xEventGroupSync(
    xEventGroup,
    MY_TASK_BIT,      // 내가 설정할 비트
    ALL_TASK_BITS,    // 모두 설정되길 기다릴 비트
    portMAX_DELAY
);
```

---

## 📋 API 요약

| API | 설명 |
|-----|------|
| `xEventGroupCreate()` | Event Group 생성 |
| `xEventGroupSetBits()` | 비트 설정 |
| `xEventGroupClearBits()` | 비트 클리어 |
| `xEventGroupWaitBits()` | 비트 대기 (AND/OR) |
| `xEventGroupSync()` | Rendezvous 동기화 |
| `xEventGroupGetBits()` | 현재 비트 조회 |

---

## 🚀 사용 방법

```c
// Event Group 생성
EventGroupHandle_t xEventGroup = xEventGroupCreate();

// 비트 설정
xEventGroupSetBits(xEventGroup, BIT_SENSOR_READY);

// 모든 비트 대기 (AND)
EventBits_t uxBits = xEventGroupWaitBits(
    xEventGroup,
    BIT_SENSOR_READY | BIT_MOTOR_READY | BIT_COMM_READY,
    pdTRUE,          // 완료 후 클리어
    pdTRUE,          // AND 모드
    portMAX_DELAY
);
```

---

## 💡 핵심 정리

1. **다중 이벤트**: 여러 조건을 하나로 동기화
2. **AND/OR**: 모두 또는 하나라도 조건
3. **Rendezvous**: 여러 태스크가 한 지점에서 만남
4. **비트 수**: 24비트 (32비트 Tick) 또는 8비트
