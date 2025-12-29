# FreeRTOS 학습 예제 모음

TriCore 보드에서 FreeRTOS를 학습하기 위한 13개의 예제 모음입니다.

## 📚 예제 목록

### 🟢 Core Task Examples (기초)

| 예제 | 주제 | 핵심 API |
|------|------|----------|
| [Ex01](Ex01_MultiTask_Priority/) | 다중 태스크 우선순위 | `xTaskCreate`, `vTaskDelay` |
| [Ex02](Ex02_Task_States/) | 태스크 상태 관리 | `vTaskSuspend`, `vTaskResume` |
| [Ex03](Ex03_Periodic_Task/) | 정밀 주기 실행 | `vTaskDelayUntil` |

### 🔵 Inter-Task Communication (통신)

| 예제 | 주제 | 핵심 API |
|------|------|----------|
| [Ex04](Ex04_Queue/) | Queue (Producer/Consumer) | `xQueueSend`, `xQueueReceive` |
| [Ex05](Ex05_Binary_Semaphore/) | Binary Semaphore | `xSemaphoreCreateBinary` |
| [Ex06](Ex06_Counting_Semaphore/) | Counting Semaphore | `xSemaphoreCreateCounting` |
| [Ex07](Ex07_Mutex/) | Mutex (Priority Inheritance) | `xSemaphoreCreateMutex` |

### 🟡 Advanced Features (고급)

| 예제 | 주제 | 핵심 API |
|------|------|----------|
| [Ex08](Ex08_Software_Timer/) | Software Timer | `xTimerCreate`, `xTimerStart` |
| [Ex09](Ex09_Event_Groups/) | Event Groups | `xEventGroupWaitBits` |
| [Ex10](Ex10_Task_Notification/) | Task Notification | `xTaskNotifyGive` |

### 🔴 Practical Demos (실전)

| 예제 | 주제 | 적용 기술 |
|------|------|-----------|
| [Ex11](Ex11_LED_Pattern/) | LED 패턴 컨트롤러 | Queue + Timer + State Machine |
| [Ex12](Ex12_Button_Debounce/) | 버튼 디바운싱 | Timer + Queue + Event |
| [Ex13](Ex13_UART_Queue/) | UART 통신 | ISR + Queue + Parser |

---

## 🚀 시작하기

### 1단계: 예제 선택
```
권장 순서: Ex01 → Ex02 → Ex03 → Ex04 → Ex05 → ... → Ex13
```

### 2단계: 코드 통합
```c
// Cpu0_Main.c에서
void core0_main(void)
{
    DAVE_Init();
    
    Ex01_RunDemo();  // 원하는 예제의 RunDemo 함수 호출
    
    vTaskStartScheduler();
    while(1) { }
}
```

### 3단계: 빌드 및 실행
1. DAVE IDE에서 프로젝트 빌드
2. 보드에 다운로드
3. LED 동작 확인

---

## 📁 폴더 구조

```
Examples/
├── Ex01_MultiTask_Priority/
│   └── Ex01_MultiTask.c
├── Ex02_Task_States/
│   └── Ex02_Task_States.c
├── Ex03_Periodic_Task/
│   └── Ex03_Periodic_Task.c
├── Ex04_Queue/
│   └── Ex04_Queue.c
├── Ex05_Binary_Semaphore/
│   └── Ex05_Binary_Semaphore.c
├── Ex06_Counting_Semaphore/
│   └── Ex06_Counting_Semaphore.c
├── Ex07_Mutex/
│   └── Ex07_Mutex.c
├── Ex08_Software_Timer/
│   └── Ex08_Software_Timer.c
├── Ex09_Event_Groups/
│   └── Ex09_Event_Groups.c
├── Ex10_Task_Notification/
│   └── Ex10_Task_Notification.c
├── Ex11_LED_Pattern/
│   └── Ex11_LED_Pattern.c
├── Ex12_Button_Debounce/
│   └── Ex12_Button_Debounce.c
└── Ex13_UART_Queue/
    └── Ex13_UART_Queue.c
```

---

## 📖 상세 가이드

자세한 학습 가이드는 [Learning_Guide/FreeRTOS_TriCore_Guide_KR.md](../Learning_Guide/FreeRTOS_TriCore_Guide_KR.md)를 참조하세요.

---

## ⚙️ 하드웨어 요구사항

- **LED**: P22.5 (기본 예제용)
- **버튼**: P15.8 (Ex12용, 선택)
- **UART**: DAVE UART APP 또는 iLLD (Ex13용, 선택)

---

## 📝 라이선스

예제 코드는 학습 목적으로 자유롭게 사용, 수정 가능합니다.
FreeRTOS는 MIT 라이선스를 따릅니다.
