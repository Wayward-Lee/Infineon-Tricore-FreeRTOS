# Example 13: UART Queue 통신 데모

## 📌 학습 목표

- ISR에서 Queue로 데이터 전달
- 텍스트 명령 파서 구현
- 인터럽트 지연 처리 (Deferred Interrupt Processing)

---

## 🏗️ 시스템 아키텍처

```mermaid
graph TB
    subgraph "Hardware"
        UART["🔌 UART RX"]
    end
    
    subgraph "Interrupt"
        ISR["⚡ RX ISR<br/>xQueueSendFromISR()"]
    end
    
    subgraph "RX Queue"
        Q["📬 Character Buffer<br/>64 bytes"]
    end
    
    subgraph "Parser Task"
        PARSE["📝 Command Parser<br/>문자 수집 & 파싱"]
    end
    
    subgraph "Handler"
        H["⚙️ Command Handler<br/>명령 실행"]
    end
    
    UART -->|"char"| ISR
    ISR -->|"xQueueSendFromISR()"| Q
    Q -->|"xQueueReceive()"| PARSE
    PARSE --> H
```

---

## 🔄 Deferred Interrupt Processing

```mermaid
sequenceDiagram
    participant HW as UART HW
    participant ISR as RX ISR
    participant Q as Queue
    participant T as Parser Task
    
    HW->>ISR: 인터럽트!
    Note over ISR: 최소한의 작업만!
    ISR->>Q: xQueueSendFromISR('L')
    ISR-->>HW: 인터럽트 종료
    
    HW->>ISR: 인터럽트!
    ISR->>Q: xQueueSendFromISR('E')
    
    HW->>ISR: 인터럽트!
    ISR->>Q: xQueueSendFromISR('D')
    
    Note over T: Queue에서 문자 수집
    Q-->>T: 'L', 'E', 'D', ' ', 'O', 'N', '\r'
    T->>T: 명령 파싱: "LED ON"
    T->>T: 명령 실행: LED 켜기
```

---

## 📊 ISR 처리 시간 비교

```mermaid
graph TB
    subgraph "❌ 잘못된 방식"
        BAD1["ISR에서 직접 처리"]
        BAD2["긴 ISR 실행 시간"]
        BAD3["다른 인터럽트 지연"]
    end
    
    subgraph "✅ 올바른 방식 (이 예제)"
        GOOD1["ISR: Queue에 넣기만"]
        GOOD2["최소 ISR 시간"]
        GOOD3["Task에서 처리"]
    end
```

---

## 📦 명령 파서 구조

```mermaid
flowchart TD
    A["문자 수신"] --> B{"문자 종류?"}
    
    B -->|"일반 문자"| C["버퍼에 추가"]
    B -->|"\\r 또는 \\n"| D["명령 완료"]
    B -->|"Backspace"| E["마지막 문자 삭제"]
    
    D --> F["명령 파싱"]
    F --> G{"명령 종류?"}
    
    G -->|"LED ON"| H["LED 켜기"]
    G -->|"LED OFF"| I["LED 끄기"]
    G -->|"STATUS"| J["상태 출력"]
    G -->|"HELP"| K["도움말"]
    G -->|"Unknown"| L["에러 메시지"]
```

---

## 🔍 ISR 코드 분석

### FromISR API 사용

```c
void vUART_RxISR(uint8_t ucReceivedByte)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    char cChar = (char)ucReceivedByte;
    
    // ISR에서는 반드시 FromISR 버전!
    xQueueSendFromISR(xRxQueue, 
                      &cChar, 
                      &xHigherPriorityTaskWoken);
    
    // 더 높은 우선순위 태스크가 깨어났으면 컨텍스트 스위칭
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### 파서 태스크

```c
void prvCommandParserTask(void *pvParameters)
{
    char cReceivedChar;
    char acCmdBuffer[CMD_BUFFER_SIZE];
    uint32_t ulCmdIndex = 0;
    
    for(;;)
    {
        // Queue에서 문자 수신 (블로킹)
        if(xQueueReceive(xRxQueue, &cReceivedChar, portMAX_DELAY) == pdPASS)
        {
            if(cReceivedChar == '\r' || cReceivedChar == '\n')
            {
                // 명령 완료 → 파싱 및 실행
                acCmdBuffer[ulCmdIndex] = '\0';
                CommandType_t eCmd = prvParseCommand(acCmdBuffer);
                prvExecuteCommand(eCmd);
                ulCmdIndex = 0;
            }
            else
            {
                // 버퍼에 추가
                if(ulCmdIndex < CMD_BUFFER_SIZE - 1)
                {
                    acCmdBuffer[ulCmdIndex++] = cReceivedChar;
                }
            }
        }
    }
}
```

---

## ⚠️ ISR API 규칙

```mermaid
graph TB
    subgraph "✅ ISR에서 사용 가능"
        Y1["xQueueSendFromISR()"]
        Y2["xSemaphoreGiveFromISR()"]
        Y3["xTaskNotifyFromISR()"]
        Y4["xTimerStartFromISR()"]
    end
    
    subgraph "❌ ISR에서 사용 금지"
        N1["xQueueSend()"]
        N2["xSemaphoreTake()"]
        N3["vTaskDelay()"]
        N4["블로킹 API 모두"]
    end
```

> [!CAUTION]
> ISR에서 블로킹 API를 호출하면 시스템이 멈춥니다!

---

## 📋 지원 명령어

| 명령 | 설명 | 응답 |
|------|------|------|
| `LED ON` | LED 켜기 | "LED is ON" |
| `LED OFF` | LED 끄기 | "LED is OFF" |
| `LED TOGGLE` | LED 토글 | "LED toggled" |
| `STATUS` | 현재 상태 | 상태 정보 |
| `HELP` | 도움말 | 명령어 목록 |

---

## 🔄 문자 vs 라인 수준 Queue

```mermaid
graph LR
    subgraph "문자 수준 (이 예제)"
        C1["ISR: 1문자씩 Queue"]
        C2["Task: 라인 조립"]
        C3["장점: 유연함"]
        C4["단점: 파싱 복잡"]
    end
    
    subgraph "라인 수준 (대안)"
        L1["ISR: 버퍼에 수집"]
        L2["\\n 시 라인 전체 Queue"]
        L3["장점: 파싱 단순"]
        L4["단점: ISR 복잡"]
    end
```

---

## 🚀 실제 적용 시 고려사항

```mermaid
graph TB
    subgraph "프로토콜 설계"
        P1["프레이밍<br/>STX/ETX"]
        P2["체크섬<br/>CRC"]
        P3["에러 처리"]
    end
    
    subgraph "버퍼 관리"
        B1["오버플로우 처리"]
        B2["타임아웃"]
        B3["버퍼 크기 설계"]
    end
    
    subgraph "DMA (고급)"
        D1["ISR 부하 감소"]
        D2["대량 데이터"]
    end
```

---

## 💡 핵심 정리

1. **ISR 최소화**: Queue에 넣기만 하고 빠르게 반환
2. **FromISR**: ISR에서는 반드시 FromISR 버전 사용
3. **Deferred Processing**: 복잡한 처리는 Task에서
4. **버퍼링**: Queue로 데이터 손실 방지
