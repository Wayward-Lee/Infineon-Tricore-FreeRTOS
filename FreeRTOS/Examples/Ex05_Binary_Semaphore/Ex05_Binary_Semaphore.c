/**********************************************************************************************************************
 * @file    Ex05_Binary_Semaphore.c
 * @brief   FreeRTOS Example 05: Binary Semaphore Demo (Event Synchronization)
 * 
 * @description
 * 이 예제는 Binary Semaphore를 사용한 이벤트 동기화를 학습합니다.
 * ISR에서 태스크를 깨우는 전형적인 패턴을 시뮬레이션합니다.
 * 
 * @learning_objectives
 * - xSemaphoreCreateBinary()로 Binary Semaphore 생성
 * - xSemaphoreGive()와 xSemaphoreTake()의 동기화 동작
 * - Binary Semaphore와 Mutex의 차이점 이해
 * - ISR-Task 동기화 패턴
 * 
 * @synchronization_pattern
 *     
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │                    Binary Semaphore 동작                        │
 *     ├─────────────────────────────────────────────────────────────────┤
 *     │                                                                 │
 *     │  Signaling Task        Semaphore         Handler Task          │
 *     │       │                   [0]                   │               │
 *     │       │                    │                    │               │
 *     │       ├── Give ──────────>│[1]                  │               │
 *     │       │                    │                    │               │
 *     │       │                    │<──── Take ─────────┤               │
 *     │       │                   [0]                   │               │
 *     │       │                    │                    ├── LED 토글    │
 *     │       │                    │                    │               │
 *     │                                                                 │
 *     │  Binary: 값은 0 또는 1만 가능                                   │
 *     │  여러번 Give해도 값은 1 유지 (카운트 안됨)                      │
 *     │                                                                 │
 *     └─────────────────────────────────────────────────────────────────┘
 * 
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxPort.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* 시그널 주기 */
#define SIGNAL_PERIOD_MS    1000    /* 1초마다 시그널 발생 */

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* Binary Semaphore 핸들 */
static SemaphoreHandle_t xBinarySemaphore = NULL;

/* 태스크 핸들 */
static TaskHandle_t xSignalingTaskHandle = NULL;
static TaskHandle_t xHandlerTaskHandle = NULL;

/* 통계 */
static volatile uint32_t g_ulSignalCount = 0;   /* 시그널 발생 횟수 */
static volatile uint32_t g_ulHandledCount = 0;  /* 처리 완료 횟수 */

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvSignalingTask(void *pvParameters);
static void prvHandlerTask(void *pvParameters);
static void prvToggleLED(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief Signaling Task - 이벤트 발생 시뮬레이션
 * 
 * 실제 프로젝트에서는 이 역할을 ISR(인터럽트 서비스 루틴)이 담당합니다.
 * 버튼 누름, 데이터 수신, 타이머 만료 등의 이벤트를 시뮬레이션합니다.
 */
static void prvSignalingTask(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        /* 이벤트가 발생할 때까지 대기 (시뮬레이션) */
        vTaskDelay(pdMS_TO_TICKS(SIGNAL_PERIOD_MS));
        
        /*---------------------------------------------------------------------
         * xSemaphoreGive(): 세마포어 해제 (시그널 발생)
         * 
         * Binary Semaphore에서:
         * - 값이 0이면 → 1로 변경, 대기 중인 태스크 깨움
         * - 값이 이미 1이면 → 그대로 1 유지 (카운트 안됨!)
         * 
         * 반환값:
         * - pdPASS: 항상 성공 (Binary Semaphore의 경우)
         * 
         * 주의: ISR에서는 xSemaphoreGiveFromISR() 사용!
         *---------------------------------------------------------------------*/
        xSemaphoreGive(xBinarySemaphore);
        
        g_ulSignalCount++;
        
        /* 
         * Binary Semaphore 특성 데모:
         * 빠르게 연속으로 Give를 해도 Handler는 한 번만 깨어남
         */
        /* xSemaphoreGive(xBinarySemaphore);  // 이미 1이므로 효과 없음 */
    }
}

/**
 * @brief Handler Task - 이벤트 처리
 * 
 * 세마포어를 대기하다가 시그널을 받으면 깨어나서 이벤트를 처리합니다.
 * 이 패턴은 ISR의 실행 시간을 최소화하는 "Deferred Interrupt Handling"에 사용됩니다.
 */
static void prvHandlerTask(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        /*---------------------------------------------------------------------
         * xSemaphoreTake(): 세마포어 획득 (이벤트 대기)
         * 
         * 파라미터:
         * - xBinarySemaphore: 대상 세마포어
         * - portMAX_DELAY: 무한 대기 (시그널 올 때까지)
         * 
         * 동작:
         * - 값이 1이면 → 0으로 변경하고 즉시 반환 (pdPASS)
         * - 값이 0이면 → Blocked 상태로 대기
         * 
         * 반환값:
         * - pdPASS: 세마포어 획득 성공
         * - pdFAIL: 타임아웃 (시간 내에 획득 실패)
         *---------------------------------------------------------------------*/
        if(xSemaphoreTake(xBinarySemaphore, portMAX_DELAY) == pdPASS)
        {
            /* 이벤트 처리 */
            g_ulHandledCount++;
            
            /* LED 토글로 이벤트 처리 표시 */
            prvToggleLED();
            
            /* 
             * 실제 프로젝트에서의 처리 예:
             * - 수신된 데이터 파싱
             * - 상태 머신 업데이트
             * - 알람 처리
             * - 로그 기록
             */
        }
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 05 데모 시작 함수
 */
void Ex05_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /*-------------------------------------------------------------------------
     * xSemaphoreCreateBinary(): Binary Semaphore 생성
     * 
     * 특징:
     * - 초기 상태: Empty(0) - 첫 Take는 블로킹됨
     * - 최대 값: 1 (여러번 Give해도 1 유지)
     * 
     * 반환값:
     * - 성공: Semaphore 핸들
     * - 실패: NULL (메모리 부족)
     * 
     * vs Mutex:
     * - Binary Semaphore: 동기화/시그널링 용도
     * - Mutex: 상호 배제(자원 보호) 용도, Priority Inheritance 지원
     *-------------------------------------------------------------------------*/
    xBinarySemaphore = xSemaphoreCreateBinary();
    
    if(xBinarySemaphore != NULL)
    {
        /* Handler Task 생성 (높은 우선순위) */
        xTaskCreate(prvHandlerTask,
                    "Handler",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    tskIDLE_PRIORITY + 2,
                    &xHandlerTaskHandle);
        
        /* Signaling Task 생성 (낮은 우선순위) */
        xTaskCreate(prvSignalingTask,
                    "Signaling",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    tskIDLE_PRIORITY + 1,
                    &xSignalingTaskHandle);
    }
}

/*********************************************************************************************************************/
/*----------------------------------------------ISR Example (Template)-----------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief ISR에서 세마포어 Give 예시
 * 
 * 실제 인터럽트 핸들러에서 태스크를 깨우는 패턴입니다.
 * 이 함수는 인터럽트 맥락에서 호출되어야 합니다.
 */
void vISR_EventHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(xBinarySemaphore != NULL)
    {
        /*---------------------------------------------------------------------
         * xSemaphoreGiveFromISR(): ISR 전용 Give 함수
         * 
         * 차이점 (vs xSemaphoreGive):
         * - 블로킹 불가
         * - pxHigherPriorityTaskWoken 파라미터로 컨텍스트 스위칭 결정
         * 
         * 반환값:
         * - pdPASS: Give 성공
         * - errQUEUE_FULL: 이미 값이 1 (Binary에서) 또는 Counting에서 가득 참
         *---------------------------------------------------------------------*/
        xSemaphoreGiveFromISR(xBinarySemaphore, &xHigherPriorityTaskWoken);
        
        /* 
         * 더 높은 우선순위 태스크가 깨어났으면 컨텍스트 스위칭 요청
         * 이렇게 하면 ISR 종료 후 즉시 Handler Task가 실행됨
         */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief 통계 조회
 */
void vGetSemaphoreStats(uint32_t *pulSignals, uint32_t *pulHandled)
{
    if(pulSignals != NULL) *pulSignals = g_ulSignalCount;
    if(pulHandled != NULL) *pulHandled = g_ulHandledCount;
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. Binary Semaphore vs Mutex
 *    ┌───────────────────┬───────────────────┬───────────────────┐
 *    │ 특성              │ Binary Semaphore  │ Mutex             │
 *    ├───────────────────┼───────────────────┼───────────────────┤
 *    │ 용도              │ 동기화/시그널링   │ 자원 보호(상호배제)│
 *    │ Give/Take 태스크  │ 다를 수 있음      │ 반드시 같아야 함  │
 *    │ Priority Inherit. │ 없음              │ 있음              │
 *    │ 초기 상태         │ Empty (0)         │ Full (1)          │
 *    │ 재귀 호출         │ 불가능            │ 가능 (Recursive)  │
 *    └───────────────────┴───────────────────┴───────────────────┘
 * 
 * 2. Binary Semaphore 동작
 *    초기: [0] (Empty)
 *    
 *    Give: [0] → [1]  ← 이미 1이면 변화 없음
 *    Take: [1] → [0]  ← 0이면 블로킹
 * 
 * 3. 전형적인 사용 패턴: Deferred Interrupt Processing
 *    
 *    ┌─────────────┐         ┌──────────────┐         ┌─────────────┐
 *    │ ISR         │──Give──>│ Binary Sema  │<──Take──│ Handler Task│
 *    │ (최소 코드) │         │              │         │ (실제 처리) │
 *    └─────────────┘         └──────────────┘         └─────────────┘
 *    
 *    장점:
 *    - ISR 실행 시간 최소화
 *    - Handler Task에서 복잡한 처리 가능 (API 호출 등)
 *    - 인터럽트 지연 시간 감소
 * 
 * 4. 중요 API
 *    ┌─────────────────────────────┬─────────────────────────────────┐
 *    │ 일반 태스크용               │ ISR용                           │
 *    ├─────────────────────────────┼─────────────────────────────────┤
 *    │ xSemaphoreGive()            │ xSemaphoreGiveFromISR()         │
 *    │ xSemaphoreTake()            │ xSemaphoreTakeFromISR()         │
 *    └─────────────────────────────┴─────────────────────────────────┘
 * 
 * 5. 주의사항
 *    - ISR에서 절대 xSemaphoreTake/Give() 사용 금지!
 *    - FromISR 함수의 xHigherPriorityTaskWoken 반드시 처리
 *    - Binary Semaphore는 이벤트 "발생 여부"만 전달
 *    - 여러번 Give해도 한 번만 처리됨 (카운트 필요시 Counting Semaphore)
 */
