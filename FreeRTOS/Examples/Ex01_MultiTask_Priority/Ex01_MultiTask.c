/**********************************************************************************************************************
 * @file    Ex01_MultiTask.c
 * @brief   FreeRTOS Example 01: Multiple Task Priority Demo
 * 
 * @description
 * 이 예제는 FreeRTOS의 우선순위 기반 선점형 스케줄링을 학습합니다.
 * 3개의 서로 다른 우선순위를 가진 태스크를 생성하고, 스케줄러가 어떻게
 * 우선순위에 따라 태스크를 실행하는지 관찰합니다.
 * 
 * @learning_objectives
 * - xTaskCreate()를 사용한 태스크 생성
 * - 태스크 우선순위 개념 이해
 * - 선점형 스케줄링 동작 원리 파악
 * - vTaskPrioritySet(), uxTaskPriorityGet() 사용법
 * 
 * @hardware
 * - LED: P22.5 (기존 LED)
 * - 추가 LED가 있으면 더 명확한 시각화 가능
 * 
 * @usage
 * 이 파일의 코드를 Cpu0_Main.c의 적절한 위치에 복사하여 사용하세요.
 * Ex01_RunDemo() 함수를 core0_main()에서 호출하면 됩니다.
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxPort.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* Task priorities - 숫자가 클수록 높은 우선순위 */
#define PRIORITY_HIGH       (tskIDLE_PRIORITY + 3)
#define PRIORITY_MEDIUM     (tskIDLE_PRIORITY + 2)
#define PRIORITY_LOW        (tskIDLE_PRIORITY + 1)

/* Task delays in milliseconds */
#define DELAY_HIGH_TASK     100     /* High priority: 빠른 주기 */
#define DELAY_MEDIUM_TASK   300     /* Medium priority: 중간 주기 */
#define DELAY_LOW_TASK      500     /* Low priority: 느린 주기 */

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* 태스크 핸들 - 태스크 제어에 사용 */
static TaskHandle_t xHighPriorityTaskHandle = NULL;
static TaskHandle_t xMediumPriorityTaskHandle = NULL;
static TaskHandle_t xLowPriorityTaskHandle = NULL;

/* 각 태스크의 실행 횟수 카운터 */
static volatile uint32_t g_ulHighPriorityCount = 0;
static volatile uint32_t g_ulMediumPriorityCount = 0;
static volatile uint32_t g_ulLowPriorityCount = 0;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvHighPriorityTask(void *pvParameters);
static void prvMediumPriorityTask(void *pvParameters);
static void prvLowPriorityTask(void *pvParameters);
static void prvToggleLED(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief LED 토글 함수
 * @note  실제 보드에서는 IfxPort_togglePin()으로 LED 상태를 변경합니다.
 */
static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief High Priority Task (우선순위 3)
 * 
 * 가장 높은 우선순위를 가진 태스크입니다.
 * 이 태스크가 Ready 상태가 되면 다른 낮은 우선순위 태스크들을 선점합니다.
 * 
 * @param pvParameters 사용하지 않음
 */
static void prvHighPriorityTask(void *pvParameters)
{
    (void)pvParameters;  /* 컴파일러 경고 방지 */
    
    const TickType_t xDelay = pdMS_TO_TICKS(DELAY_HIGH_TASK);
    
    for(;;)
    {
        /* 실행 횟수 증가 */
        g_ulHighPriorityCount++;
        
        /* LED 토글 - High Priority 실행 표시 */
        prvToggleLED();
        
        /* 
         * 블로킹 딜레이: 이 태스크는 Blocked 상태로 전환되어
         * 낮은 우선순위 태스크들이 실행될 기회를 얻습니다.
         */
        vTaskDelay(xDelay);
    }
}

/**
 * @brief Medium Priority Task (우선순위 2)
 * 
 * 중간 우선순위를 가진 태스크입니다.
 * High Priority 태스크가 Blocked 상태일 때만 실행됩니다.
 * Low Priority 태스크보다는 먼저 스케줄링됩니다.
 * 
 * @param pvParameters 사용하지 않음
 */
static void prvMediumPriorityTask(void *pvParameters)
{
    (void)pvParameters;
    
    const TickType_t xDelay = pdMS_TO_TICKS(DELAY_MEDIUM_TASK);
    
    for(;;)
    {
        g_ulMediumPriorityCount++;
        
        /* 
         * 이 태스크의 실행은 High Priority 태스크에 의해 
         * 언제든지 선점(preempt)될 수 있습니다.
         */
        
        vTaskDelay(xDelay);
    }
}

/**
 * @brief Low Priority Task (우선순위 1)
 * 
 * 가장 낮은 우선순위를 가진 태스크입니다.
 * 다른 모든 태스크가 Blocked 상태일 때만 실행됩니다.
 * 
 * @param pvParameters 사용하지 않음
 */
static void prvLowPriorityTask(void *pvParameters)
{
    (void)pvParameters;
    
    const TickType_t xDelay = pdMS_TO_TICKS(DELAY_LOW_TASK);
    UBaseType_t uxCurrentPriority;
    
    for(;;)
    {
        g_ulLowPriorityCount++;
        
        /* 현재 태스크의 우선순위 조회 예시 */
        uxCurrentPriority = uxTaskPriorityGet(NULL);  /* NULL = 현재 태스크 */
        (void)uxCurrentPriority;  /* 사용하지 않음 - 예시용 */
        
        vTaskDelay(xDelay);
    }
}

/*********************************************************************************************************************/
/*---------------------------------------------------Demo Functions--------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 우선순위 동적 변경 데모
 * 
 * 이 함수는 별도의 태스크에서 호출되어 동적으로 우선순위를 변경하는
 * 예시를 보여줍니다. 실제 데모에서 활용하려면 추가 태스크로 만드세요.
 */
void vDemoPriorityChange(void)
{
    /* Low Priority 태스크의 우선순위를 High로 변경 */
    if(xLowPriorityTaskHandle != NULL)
    {
        vTaskPrioritySet(xLowPriorityTaskHandle, PRIORITY_HIGH);
    }
    
    /* 잠시 후 원래 우선순위로 복원 */
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    if(xLowPriorityTaskHandle != NULL)
    {
        vTaskPrioritySet(xLowPriorityTaskHandle, PRIORITY_LOW);
    }
}

/**
 * @brief 태스크 실행 통계 조회
 * 
 * @param pulHigh   High priority 태스크 실행 횟수 반환
 * @param pulMedium Medium priority 태스크 실행 횟수 반환  
 * @param pulLow    Low priority 태스크 실행 횟수 반환
 */
void vGetTaskCounts(uint32_t *pulHigh, uint32_t *pulMedium, uint32_t *pulLow)
{
    if(pulHigh != NULL)   *pulHigh = g_ulHighPriorityCount;
    if(pulMedium != NULL) *pulMedium = g_ulMediumPriorityCount;
    if(pulLow != NULL)    *pulLow = g_ulLowPriorityCount;
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 01 데모 시작 함수
 * 
 * 이 함수를 core0_main()에서 DAVE_Init() 이후, vTaskStartScheduler() 이전에 
 * 호출하세요.
 * 
 * @code
 * void core0_main(void)
 * {
 *     // ... DAVE_Init() 등 초기화 ...
 *     
 *     Ex01_RunDemo();  // 예제 시작
 *     
 *     vTaskStartScheduler();  // 스케줄러 시작
 *     
 *     while(1) { }
 * }
 * @endcode
 */
void Ex01_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /* High Priority Task 생성 */
    xTaskCreate(prvHighPriorityTask,
                "HighTask",
                configMINIMAL_STACK_SIZE,
                NULL,
                PRIORITY_HIGH,
                &xHighPriorityTaskHandle);
    
    /* Medium Priority Task 생성 */
    xTaskCreate(prvMediumPriorityTask,
                "MediumTask",
                configMINIMAL_STACK_SIZE,
                NULL,
                PRIORITY_MEDIUM,
                &xMediumPriorityTaskHandle);
    
    /* Low Priority Task 생성 */
    xTaskCreate(prvLowPriorityTask,
                "LowTask",
                configMINIMAL_STACK_SIZE,
                NULL,
                PRIORITY_LOW,
                &xLowPriorityTaskHandle);
    
    /* 
     * 참고: vTaskStartScheduler()는 이 함수 외부에서 호출되어야 합니다.
     * 스케줄러가 시작되면 가장 높은 우선순위의 Ready 상태 태스크가 실행됩니다.
     */
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. 태스크 우선순위
 *    - FreeRTOS에서 숫자가 클수록 높은 우선순위
 *    - configMAX_PRIORITIES (FreeRTOSConfig.h에서 10으로 설정)까지 사용 가능
 *    - tskIDLE_PRIORITY (0)는 Idle 태스크용으로 예약
 * 
 * 2. 선점형 스케줄링 (configUSE_PREEMPTION = 1)
 *    - 높은 우선순위 태스크가 Ready 상태가 되면 즉시 현재 태스크를 선점
 *    - 낮은 우선순위 태스크는 높은 우선순위 태스크가 Blocked 상태일 때만 실행
 * 
 * 3. vTaskDelay()
 *    - 태스크를 Blocked 상태로 만들어 CPU를 양보
 *    - 지정된 틱 수 만큼 대기 후 Ready 상태로 전환
 * 
 * 4. 태스크 핸들 (TaskHandle_t)
 *    - xTaskCreate()에서 반환되는 핸들로 태스크 제어
 *    - vTaskPrioritySet(), vTaskSuspend() 등에서 사용
 * 
 * 5. 실행 결과 예측
 *    - 10초 후 예상 실행 횟수:
 *      High (100ms): ~100회
 *      Medium (300ms): ~33회
 *      Low (500ms): ~20회
 *    - 실제 결과는 선점 타이밍에 따라 약간 다를 수 있음
 */
