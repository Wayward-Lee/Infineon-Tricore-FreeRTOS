# Example 10: Task Notification 데모

## 📌 학습 목표

- Task Notification으로 경량 IPC 구현
- Semaphore/Queue 대비 장단점 이해
- 다양한 Notification 모드 활용

---

## 🆚 전통적 IPC vs Task Notification

```mermaid
graph LR
    subgraph "전통적 방식"
        A1["Task A"] -->|"xSemaphoreGive()"| SEM["📦 Semaphore<br/>(별도 객체)"]
        SEM -->|"xSemaphoreTake()"| A2["Task B"]
    end
    
    subgraph "Task Notification"
        B1["Task A"] -->|"xTaskNotifyGive()"| B2["Task B<br/>(TCB 내장 값)"]
    end
```

---

## 📊 성능 비교

```mermaid
graph TB
    subgraph "메모리 사용"
        M1["Semaphore: ~80 바이트"]
        M2["Task Notification: 0 바이트<br/>(TCB에 이미 포함)"]
    end
    
    subgraph "속도"
        S1["Semaphore: 100%"]
        S2["Task Notification: 약 45% 빠름"]
    end
```

| 항목 | Binary Semaphore | Task Notification |
|------|------------------|-------------------|
| 추가 메모리 | ~80 bytes | 0 bytes |
| 생성 필요 | Yes | No |
| 속도 | 기준 | ~45% 빠름 |
| N:1 통신 | 가능 | 불가능 |
| ISR 지원 | 가능 | 가능 |

---

## 🔄 Notification 동작 모드

```mermaid
graph TB
    subgraph "eNoAction"
        N1["단순 알림만"]
        N2["값 변경 없음"]
    end
    
    subgraph "eSetBits"
        B1["ulValue OR 연산"]
        B2["Event Group과 유사"]
    end
    
    subgraph "eIncrement"
        I1["값 1 증가"]
        I2["Counting Sema와 유사"]
    end
    
    subgraph "eSetValueWithOverwrite"
        O1["값 덮어쓰기"]
        O2["항상 성공"]
    end
    
    subgraph "eSetValueWithoutOverwrite"
        W1["값 설정"]
        W2["이전 값 미처리 시 실패"]
    end
```

---

## 📦 TCB 내 Notification 구조

```mermaid
classDiagram
    class TCB_t {
        +StackType_t *pxTopOfStack
        +UBaseType_t uxPriority
        +uint32_t ulNotifiedValue
        +uint8_t ucNotifyState
    }
    
    class NotifyState {
        <<enumeration>>
        taskNOT_WAITING_NOTIFICATION
        taskWAITING_NOTIFICATION
        taskNOTIFICATION_RECEIVED
    }
    
    TCB_t --> NotifyState
```

---

## ⚙️ 커널 동작 원리

### xTaskNotifyGive() 내부

```mermaid
sequenceDiagram
    participant S as Sender Task
    participant K as Kernel
    participant R as Receiver TCB
    
    S->>K: xTaskNotifyGive(xReceiverHandle)
    K->>R: ulNotifiedValue++
    K->>K: ucNotifyState 확인
    
    alt WAITING_NOTIFICATION
        K->>R: ucNotifyState = RECEIVED
        K->>R: Ready 리스트로 이동
        K->>K: 컨텍스트 스위칭 필요?
    else NOT_WAITING
        Note over K: 나중에 Take할 때 확인됨
    end
    
    K-->>S: pdPASS
```

### ulTaskNotifyTake() 내부

```mermaid
flowchart TD
    A["ulTaskNotifyTake(xClearOnExit, xTicksToWait)"] --> B{"ulNotifiedValue > 0?"}
    B -->|Yes| C{"xClearOnExit?"}
    C -->|pdTRUE| D["ulNotifiedValue = 0<br/>(Binary 동작)"]
    C -->|pdFALSE| E["ulNotifiedValue--<br/>(Counting 동작)"]
    D --> F["값 반환"]
    E --> F
    
    B -->|No| G{"xTicksToWait > 0?"}
    G -->|Yes| H["ucNotifyState = WAITING"]
    H --> I["블로킹"]
    G -->|No| J["0 반환"]
```

---

## 🔍 커널 코드 분석

### xTaskGenericNotify() (tasks.c)

```c
BaseType_t xTaskGenericNotify(TaskHandle_t xTaskToNotify,
                               uint32_t ulValue,
                               eNotifyAction eAction,
                               uint32_t *pulPreviousNotificationValue)
{
    TCB_t *pxTCB = xTaskToNotify;
    
    taskENTER_CRITICAL();
    {
        if(pulPreviousNotificationValue != NULL)
        {
            *pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
        }
        
        // Action에 따른 값 변경
        switch(eAction)
        {
            case eSetBits:
                pxTCB->ulNotifiedValue |= ulValue;
                break;
                
            case eIncrement:
                pxTCB->ulNotifiedValue++;
                break;
                
            case eSetValueWithOverwrite:
                pxTCB->ulNotifiedValue = ulValue;
                break;
                
            case eSetValueWithoutOverwrite:
                if(pxTCB->ucNotifyState != taskNOTIFICATION_RECEIVED)
                {
                    pxTCB->ulNotifiedValue = ulValue;
                }
                else
                {
                    xReturn = pdFAIL;
                }
                break;
                
            case eNoAction:
                break;
        }
        
        // 대기 중이면 깨움
        if(pxTCB->ucNotifyState == taskWAITING_NOTIFICATION)
        {
            pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;
            prvAddTaskToReadyList(pxTCB);
            
            if(pxTCB->uxPriority > pxCurrentTCB->uxPriority)
            {
                taskYIELD_IF_USING_PREEMPTION();
            }
        }
    }
    taskEXIT_CRITICAL();
    
    return xReturn;
}
```

---

## 📋 전통적 IPC 대체 패턴

```mermaid
graph TB
    subgraph "Binary Semaphore 대체"
        BS1["xTaskNotifyGive()"]
        BS2["ulTaskNotifyTake(pdTRUE, ...)"]
    end
    
    subgraph "Counting Semaphore 대체"
        CS1["xTaskNotifyGive()"]
        CS2["ulTaskNotifyTake(pdFALSE, ...)"]
    end
    
    subgraph "Event Group 비트 대체"
        EG1["xTaskNotify(..., eSetBits)"]
        EG2["xTaskNotifyWait()"]
    end
    
    subgraph "Mailbox 대체"
        MB1["xTaskNotify(..., eSetValueXxx)"]
        MB2["xTaskNotifyWait()"]
    end
```

---

## ⚠️ 제약 사항

```mermaid
graph LR
    subgraph "✅ 가능"
        Y1["1:1 통신"]
        Y2["ISR → Task"]
        Y3["Task → Task"]
    end
    
    subgraph "❌ 불가능"
        N1["N:1 통신<br/>(여러 태스크 대기)"]
        N2["데이터 버퍼링<br/>(Queue처럼)"]
        N3["수신자 핸들 모르는 경우"]
    end
```

---

## 🚀 사용 방법

```c
// Binary Semaphore처럼 사용
void vSenderTask(void)
{
    xTaskNotifyGive(xReceiverHandle);
}

void vReceiverTask(void)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Binary
    // 또는
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY); // Counting
}

// 값 전달
xTaskNotify(xHandle, 0x1234, eSetValueWithOverwrite);
xTaskNotifyWait(0, ULONG_MAX, &ulReceivedValue, portMAX_DELAY);
```

---

## 💡 핵심 정리

1. **경량**: 별도 객체 생성 불필요
2. **빠름**: ~45% 성능 향상
3. **1:1 전용**: 다중 대기 불가
4. **다양한 모드**: 비트/증가/값 설정
