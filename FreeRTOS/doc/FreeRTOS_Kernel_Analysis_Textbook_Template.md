# FreeRTOS 커널 심층 분석 교과서

---

**교재명**: FreeRTOS 커널 심층 분석 - TriCore 플랫폼 실전 학습서  
**버전**: 1.0  
**저자**: [저자명]  
**최종 수정일**: 2025-12-29

---

## 머리말w

본 교재는 FreeRTOS 커널의 내부 동작 원리를 심층적으로 분석하는 것을 목표로 합니다. 
단순한 API 사용법을 넘어서, 커널 소스 코드를 직접 분석하며 RTOS의 핵심 메커니즘을 이해합니다.

### 대상 독자
- 임베디드 시스템 개발자
- RTOS 내부 동작 원리를 이해하고자 하는 엔지니어
- 자동차/항공우주 등 미션 크리티컬 시스템 개발자

### 학습 환경
- 플랫폼: Infineon TriCore (TC387)
- 개발환경: DAVE IDE
- FreeRTOS 버전: v10.x (AURIX 포트)

---

## 목차

### Part I: FreeRTOS 기초 및 아키텍처

**Chapter 1: RTOS 개요 및 FreeRTOS 소개**
- 1.1 실시간 운영체제(RTOS)란?
- 1.2 FreeRTOS의 역사와 특징
- 1.3 FreeRTOS 아키텍처 개요
- 1.4 TriCore 포팅 레이어 소개

**Chapter 2: FreeRTOS 소스 코드 구조**
- 2.1 커널 소스 파일 구성
- 2.2 주요 헤더 파일 분석
- 2.3 FreeRTOSConfig.h 설정 상세
- 2.4 포터블 레이어(Portable Layer) 이해

---

### Part II: 태스크(Task) 심층 분석

**Chapter 3: 태스크 생성과 TCB (Ex01 기반)**
- 3.1 태스크란 무엇인가?
- 3.2 xTaskCreate() 커널 코드 분석
  - 3.2.1 함수 시그니처 및 파라미터
  - 3.2.2 TCB(Task Control Block) 구조체 분석
  - 3.2.3 스택 메모리 할당 과정
  - 3.2.4 초기 컨텍스트 설정
- 3.3 우선순위(Priority) 시스템 내부 구현
- 3.4 Ready List 자료구조 분석
- 3.5 실습: Ex01_MultiTask_Priority 코드 분석

**Chapter 4: 태스크 상태 전이 (Ex02 기반)**
- 4.1 태스크 상태 머신
  - 4.1.1 Ready 상태
  - 4.1.2 Running 상태
  - 4.1.3 Blocked 상태
  - 4.1.4 Suspended 상태
- 4.2 vTaskSuspend() 커널 코드 분석
- 4.3 vTaskResume() 커널 코드 분석
- 4.4 상태 전이 시나리오별 분석
- 4.5 실습: Ex02_Task_States 코드 분석

**Chapter 5: 스케줄러 핵심 메커니즘**
- 5.1 스케줄러 아키텍처 개요
- 5.2 vTaskStartScheduler() 분석
  - 5.2.1 초기화 시퀀스
  - 5.2.2 Idle Task 생성
  - 5.2.3 Timer Task 생성 (configUSE_TIMERS)
- 5.3 태스크 선택 알고리즘
  - 5.3.1 taskSELECT_HIGHEST_PRIORITY_TASK() 분석
  - 5.3.2 Leading Zeros Count 최적화
- 5.4 컨텍스트 스위칭 상세
  - 5.4.1 portSAVE_CONTEXT 분석
  - 5.4.2 portRESTORE_CONTEXT 분석
  - 5.4.3 TriCore 레지스터 세트 이해

**Chapter 6: 시간 관리 및 Tick 인터럽트 (Ex03 기반)**
- 6.1 Tick 인터럽트 메커니즘
- 6.2 xPortSysTickHandler() 분석
- 6.3 xTaskIncrementTick() 커널 코드 분석
- 6.4 vTaskDelay() vs vTaskDelayUntil()
  - 6.4.1 상대적 지연 vs 절대적 주기
  - 6.4.2 커널 내부 구현 비교
- 6.5 Delayed Task List 관리
- 6.6 실습: Ex03_Periodic_Task 코드 분석

---

### Part III: 태스크 간 통신(IPC) 심층 분석

**Chapter 7: Queue 메커니즘 (Ex04 기반)**
- 7.1 Queue 자료구조 내부 구현
  - 7.1.1 Queue_t 구조체 분석
  - 7.1.2 원형 버퍼(Circular Buffer) 구현
- 7.2 xQueueCreate() 커널 코드 분석
- 7.3 xQueueSend() 커널 코드 분석
  - 7.3.1 블로킹 메커니즘
  - 7.3.2 대기 태스크 깨우기
- 7.4 xQueueReceive() 커널 코드 분석
- 7.5 ISR에서의 Queue 사용: FromISR 변형
- 7.6 실습: Ex04_Queue 코드 분석 (Producer-Consumer 패턴)

**Chapter 8: Binary Semaphore (Ex05 기반)**
- 8.1 Semaphore의 개념과 역사
- 8.2 Binary Semaphore vs Counting Semaphore
- 8.3 xSemaphoreCreateBinary() 내부 구현
  - 8.3.1 Queue 기반 Semaphore 구현 이유
- 8.4 xSemaphoreGive() 분석
- 8.5 xSemaphoreTake() 분석
  - 8.5.1 타임아웃 메커니즘
  - 8.5.2 블로킹 처리
- 8.6 ISR-Task 동기화 패턴
- 8.7 실습: Ex05_Binary_Semaphore 코드 분석

**Chapter 9: Counting Semaphore (Ex06 기반)**
- 9.1 Counting Semaphore 사용 시나리오
  - 9.1.1 리소스 풀 관리
  - 9.1.2 이벤트 카운팅
- 9.2 xSemaphoreCreateCounting() 구현 분석
- 9.3 실습: Ex06_Counting_Semaphore 코드 분석

**Chapter 10: Mutex와 우선순위 역전 해결 (Ex07 기반)**
- 10.1 Mutex vs Semaphore
- 10.2 우선순위 역전(Priority Inversion) 문제
  - 10.2.1 문제 시나리오
  - 10.2.2 Mars Pathfinder 사례
- 10.3 우선순위 상속(Priority Inheritance) 메커니즘
  - 10.3.1 xTaskPriorityInherit() 커널 코드 분석
  - 10.3.2 xTaskPriorityDisinherit() 커널 코드 분석
- 10.4 xSemaphoreCreateMutex() 구현 분석
- 10.5 Recursive Mutex
- 10.6 실습: Ex07_Mutex 코드 분석

---

### Part IV: 고급 기능 심층 분석

**Chapter 11: Software Timer (Ex08 기반)**
- 11.1 Software Timer 아키텍처
- 11.2 Timer Daemon Task
  - 11.2.1 prvTimerTask() 분석
  - 11.2.2 Timer Command Queue
- 11.3 xTimerCreate() 커널 코드 분석
- 11.4 xTimerStart() / xTimerStop() 분석
- 11.5 One-shot vs Periodic Timer
- 11.6 실습: Ex08_Software_Timer 코드 분석

**Chapter 12: Event Groups (Ex09 기반)**
- 12.1 Event Groups 사용 시나리오
- 12.2 EventGroup_t 구조체 분석
- 12.3 xEventGroupCreate() 분석
- 12.4 xEventGroupSetBits() 분석
- 12.5 xEventGroupWaitBits() 분석
  - 12.5.1 AND/OR 대기 메커니즘
  - 12.5.2 자동 클리어 옵션
- 12.6 실습: Ex09_Event_Groups 코드 분석 (다중 센서 동기화)

**Chapter 13: Task Notification (Ex10 기반)**
- 13.1 Task Notification의 장점
  - 13.1.1 메모리 효율성
  - 13.1.2 성능 이점
- 13.2 TCB 내 Notification 필드 분석
- 13.3 xTaskNotifyGive() / ulTaskNotifyTake() 분석
- 13.4 xTaskNotify() / xTaskNotifyWait() 분석
- 13.5 Direct-to-Task Notification 패턴
- 13.6 실습: Ex10_Task_Notification 코드 분석

---

### Part V: 실전 응용 및 통합

**Chapter 14: LED 패턴 컨트롤러 (Ex11 기반)**
- 14.1 요구사항 분석
- 14.2 시스템 아키텍처 설계
- 14.3 상태 머신 구현
- 14.4 Queue + Timer 통합 패턴
- 14.5 실습: Ex11_LED_Pattern 전체 분석

**Chapter 15: 버튼 디바운싱 시스템 (Ex12 기반)**
- 15.1 하드웨어 바운싱 현상
- 15.2 소프트웨어 디바운싱 기법
- 15.3 Timer + Queue + Event 통합
- 15.4 실습: Ex12_Button_Debounce 전체 분석

**Chapter 16: UART 통신 시스템 (Ex13 기반)**
- 16.1 ISR 기반 통신 아키텍처
- 16.2 Ring Buffer와 Queue 통합
- 16.3 프로토콜 파서 구현
- 16.4 실습: Ex13_UART_Queue 전체 분석

---

### Part VI: 커널 내부 고급 분석

**Chapter 17: 메모리 관리**
- 17.1 힙 메모리 관리 구조
- 17.2 heap_1.c ~ heap_5.c 비교 분석
- 17.3 pvPortMalloc() / vPortFree() 상세
- 17.4 메모리 단편화 문제와 해결책
- 17.5 MPU(Memory Protection Unit) 지원

**Chapter 18: 인터럽트 및 컨텍스트 관리**
- 18.1 Critical Section 구현
  - 18.1.1 taskENTER_CRITICAL()
  - 18.1.2 taskEXIT_CRITICAL()
  - 18.1.3 인터럽트 중첩 관리
- 18.2 TriCore 인터럽트 시스템
- 18.3 Deferred Interrupt Processing 패턴
- 18.4 ISR에서의 Context Switch 요청

**Chapter 19: 디버깅 및 최적화**
- 19.1 스택 오버플로우 검출 메커니즘
- 19.2 Run-time Statistics 수집
- 19.3 Trace 기능 활용
- 19.4 성능 최적화 기법
  - 19.4.1 configUSE_LIST_DATA_INTEGRITY_CHECK
  - 19.4.2 configASSERT() 활용
- 19.5 일반적인 버그 패턴과 해결책

---

### 부록

**Appendix A: FreeRTOS 핵심 자료구조**
- A.1 List_t / ListItem_t 구조체
- A.2 TCB_t (Task Control Block) 전체 필드
- A.3 Queue_t 구조체 전체 필드

**Appendix B: TriCore 포트 레이어 분석**
- B.1 port.c 핵심 함수
- B.2 portmacro.h 매크로 정의
- B.3 CSA(Context Save Area) 관리

**Appendix C: FreeRTOS API 빠른 참조**
- C.1 Task API
- C.2 Queue API
- C.3 Semaphore/Mutex API
- C.4 Timer API
- C.5 Event Group API
- C.6 Task Notification API

**Appendix D: 용어 사전(Glossary)**

---

## 챕터 작성 템플릿

각 챕터는 다음 구조를 따릅니다:

### [Chapter X: 제목]

#### 학습 목표
이 챕터를 완료하면 다음을 할 수 있습니다:
- [ ] 목표 1
- [ ] 목표 2
- [ ] 목표 3

#### X.1 개요
(해당 주제에 대한 개념적 설명)

#### X.2 커널 코드 분석

**파일 위치**: `FreeRTOS-Kernel/tasks.c`

```c
// 분석 대상 함수 전체 소스코드
BaseType_t xTaskCreate( ... )
{
    // 상세 주석과 함께
}
```

**라인별 분석**:

| 라인 | 코드 | 설명 |
|------|------|------|
| 100 | `prvInitialiseTaskLists();` | Ready List 초기화 |
| 101 | ... | ... |

#### X.3 시퀀스 다이어그램

```
호출자 → xTaskCreate() → prvAllocateTCBAndStack() → pxPortInitialiseStack()
                      → prvAddTaskToReadyList() → 반환
```

#### X.4 실습 예제 분석

**예제 코드**: `Examples/ExXX_XXX/ExXX_XXX.c`

```c
// 예제 코드 전체
```

**실행 결과 분석**:
- 예상 LED 동작
- 타이밍 분석

#### X.5 핵심 정리
- 핵심 포인트 1
- 핵심 포인트 2
- 핵심 포인트 3

#### X.6 연습 문제
1. [이해 확인 문제]
2. [코드 분석 문제]
3. [응용 문제]

---

## 문서 스타일 가이드

### 코드 블록 형식
- 커널 코드: 라인 번호 포함, 상세 주석
- 예제 코드: 완전한 실행 가능 코드
- 분석 테이블: 라인별 또는 블록별 설명

### 다이어그램 형식
- 상태 다이어그램: ASCII 아트 또는 Mermaid
- 시퀀스 다이어그램: 함수 호출 흐름
- 구조 다이어그램: 자료구조 레이아웃

### 강조 표시
- **핵심 용어**: 볼드체
- `함수명()`: 코드 형식
- > [!IMPORTANT]: 중요 주의사항
- > [!TIP]: 팁 및 최적화 제안

---

**문서 끝**
