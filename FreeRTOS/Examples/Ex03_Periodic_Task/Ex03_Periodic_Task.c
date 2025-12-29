/**********************************************************************************************************************
 * @file    Ex03_Periodic_Task.c
 * @brief   FreeRTOS Example 03: Periodic Task Demo (vTaskDelay vs vTaskDelayUntil)
 * 
 * @description
 * 이 예제는 정확한 주기적 실행을 위한 두 가지 딜레이 방식을 비교합니다.
 * - vTaskDelay(): 상대적 딜레이 (실행 시간 누적으로 주기 흔들림 발생)
 * - vTaskDelayUntil(): 절대적 딜레이 (정확한 주기 유지)
 * 
 * @learning_objectives
 * - vTaskDelay()와 vTaskDelayUntil()의 차이점 이해
 * - 정밀한 주기적 태스크 구현 방법
 * - xTaskGetTickCount()로 시간 측정
 * 
 * @timing_comparison
 * 
 *     vTaskDelay(100ms)              vTaskDelayUntil(100ms)
 *     ─────────────────              ──────────────────────
 *     
 *     [실행 10ms][딜레이 100ms]      [실행 10ms][딜레이 90ms]
 *     주기 = 110ms (흔들림)          주기 = 100ms (정확)
 *     
 *     [실행 20ms][딜레이 100ms]      [실행 20ms][딜레이 80ms]
 *     주기 = 120ms (흔들림)          주기 = 100ms (정확)
 *     
 *     ▲ 실행시간 변동이 주기에 영향   ▲ 실행시간 변동을 보상
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

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* 주기 설정 (밀리초) */
#define TASK_PERIOD_MS      100

/* 시뮬레이션할 작업 부하 (밀리초) */
#define SIMULATED_WORK_MS   10

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
static TaskHandle_t xRelativeDelayTaskHandle = NULL;
static TaskHandle_t xAbsoluteDelayTaskHandle = NULL;

/* 주기 측정용 변수 */
static volatile TickType_t g_xRelativeLastWakeTime = 0;
static volatile TickType_t g_xAbsoluteLastWakeTime = 0;

/* 최대 주기 편차 기록 */
static volatile TickType_t g_xRelativeMaxDeviation = 0;
static volatile TickType_t g_xAbsoluteMaxDeviation = 0;

/* 실행 횟수 카운터 */
static volatile uint32_t g_ulRelativeCount = 0;
static volatile uint32_t g_ulAbsoluteCount = 0;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvRelativeDelayTask(void *pvParameters);
static void prvAbsoluteDelayTask(void *pvParameters);
static void prvSimulateWork(uint32_t ulWorkTimeMs);
static void prvToggleLED(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief 작업 부하 시뮬레이션
 * 
 * 실제 프로젝트에서는 센서 읽기, 계산, 통신 등이 이 부분에 해당합니다.
 * 이 함수는 의도적으로 CPU 시간을 소비합니다.
 */
static void prvSimulateWork(uint32_t ulWorkTimeMs)
{
    TickType_t xStartTime = xTaskGetTickCount();
    TickType_t xTargetTicks = pdMS_TO_TICKS(ulWorkTimeMs);
    
    /* Busy-wait로 작업 시간 시뮬레이션 */
    while((xTaskGetTickCount() - xStartTime) < xTargetTicks)
    {
        /* CPU 사용 중... */
    }
}

/**
 * @brief 상대적 딜레이 태스크 (vTaskDelay 사용)
 * 
 * vTaskDelay()는 현재 시점부터 지정된 시간만큼 대기합니다.
 * 따라서 태스크 실행 시간이 주기에 포함되어 전체 주기가 늘어납니다.
 * 
 * 예: 100ms 딜레이 + 10ms 실행 = 110ms 주기
 */
static void prvRelativeDelayTask(void *pvParameters)
{
    (void)pvParameters;
    
    const TickType_t xDelay = pdMS_TO_TICKS(TASK_PERIOD_MS);
    TickType_t xCurrentTime;
    TickType_t xActualPeriod;
    TickType_t xExpectedPeriod = pdMS_TO_TICKS(TASK_PERIOD_MS);
    TickType_t xDeviation;
    
    /* 첫 실행 시간 기록 */
    g_xRelativeLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        /* 현재 시간 기록 */
        xCurrentTime = xTaskGetTickCount();
        
        /*-----------------------------------------------------------------
         * 실제 주기 측정 및 편차 계산
         *-----------------------------------------------------------------*/
        if(g_ulRelativeCount > 0)
        {
            xActualPeriod = xCurrentTime - g_xRelativeLastWakeTime;
            
            /* 편차 계산 (절대값) */
            if(xActualPeriod > xExpectedPeriod)
            {
                xDeviation = xActualPeriod - xExpectedPeriod;
            }
            else
            {
                xDeviation = xExpectedPeriod - xActualPeriod;
            }
            
            /* 최대 편차 업데이트 */
            if(xDeviation > g_xRelativeMaxDeviation)
            {
                g_xRelativeMaxDeviation = xDeviation;
            }
        }
        
        g_xRelativeLastWakeTime = xCurrentTime;
        g_ulRelativeCount++;
        
        /*-----------------------------------------------------------------
         * 작업 수행 (가변 실행 시간)
         *-----------------------------------------------------------------*/
        prvSimulateWork(SIMULATED_WORK_MS);
        
        /*-----------------------------------------------------------------
         * vTaskDelay(): 상대적 딜레이
         * 
         * 현재 시점에서 100ms 후에 Ready 상태로 전환
         * 실행 시간 10ms + 딜레이 100ms = 총 주기 110ms
         *-----------------------------------------------------------------*/
        vTaskDelay(xDelay);
    }
}

/**
 * @brief 절대적 딜레이 태스크 (vTaskDelayUntil 사용)
 * 
 * vTaskDelayUntil()은 마지막 깨어난 시간 기준으로 다음 깨어날 시간을 계산합니다.
 * 따라서 태스크 실행 시간에 관계없이 정확한 주기를 유지합니다.
 * 
 * 예: 100ms 주기 설정 → 실행 시간과 무관하게 100ms 주기 유지
 */
static void prvAbsoluteDelayTask(void *pvParameters)
{
    (void)pvParameters;
    
    const TickType_t xPeriod = pdMS_TO_TICKS(TASK_PERIOD_MS);
    TickType_t xLastWakeTime;
    TickType_t xCurrentTime;
    TickType_t xActualPeriod;
    TickType_t xExpectedPeriod = pdMS_TO_TICKS(TASK_PERIOD_MS);
    TickType_t xDeviation;
    
    /*---------------------------------------------------------------------
     * 중요: xLastWakeTime 초기화
     * vTaskDelayUntil()을 처음 호출하기 전에 현재 틱 카운트로 초기화해야 함
     *---------------------------------------------------------------------*/
    xLastWakeTime = xTaskGetTickCount();
    g_xAbsoluteLastWakeTime = xLastWakeTime;
    
    for(;;)
    {
        /* 현재 시간 기록 */
        xCurrentTime = xTaskGetTickCount();
        
        /*-----------------------------------------------------------------
         * 실제 주기 측정 및 편차 계산
         *-----------------------------------------------------------------*/
        if(g_ulAbsoluteCount > 0)
        {
            xActualPeriod = xCurrentTime - g_xAbsoluteLastWakeTime;
            
            /* 편차 계산 (절대값) */
            if(xActualPeriod > xExpectedPeriod)
            {
                xDeviation = xActualPeriod - xExpectedPeriod;
            }
            else
            {
                xDeviation = xExpectedPeriod - xActualPeriod;
            }
            
            /* 최대 편차 업데이트 */
            if(xDeviation > g_xAbsoluteMaxDeviation)
            {
                g_xAbsoluteMaxDeviation = xDeviation;
            }
        }
        
        g_xAbsoluteLastWakeTime = xCurrentTime;
        g_ulAbsoluteCount++;
        
        /* LED 토글 (절대 딜레이 태스크만 LED 제어) */
        prvToggleLED();
        
        /*-----------------------------------------------------------------
         * 작업 수행 (가변 실행 시간)
         *-----------------------------------------------------------------*/
        prvSimulateWork(SIMULATED_WORK_MS);
        
        /*-----------------------------------------------------------------
         * vTaskDelayUntil(): 절대적 딜레이
         * 
         * xLastWakeTime 기준으로 정확히 100ms 후에 깨어남
         * xLastWakeTime은 함수 내부에서 자동으로 업데이트됨
         * 
         * 실행 시간이 10ms여도 총 주기는 100ms 유지
         * (딜레이가 자동으로 90ms로 조정됨)
         *-----------------------------------------------------------------*/
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 03 데모 시작 함수
 */
void Ex03_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /* 상대적 딜레이 태스크 생성 */
    xTaskCreate(prvRelativeDelayTask,
                "RelDelay",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                &xRelativeDelayTaskHandle);
    
    /* 절대적 딜레이 태스크 생성 (같은 우선순위) */
    xTaskCreate(prvAbsoluteDelayTask,
                "AbsDelay",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                &xAbsoluteDelayTaskHandle);
}

/*********************************************************************************************************************/
/*-------------------------------------------Statistics Query Functions----------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 주기 편차 통계 조회
 * 
 * @param pulRelativeDeviation 상대 딜레이 태스크의 최대 편차 (틱)
 * @param pulAbsoluteDeviation 절대 딜레이 태스크의 최대 편차 (틱)
 */
void vGetPeriodDeviations(TickType_t *pxRelativeDeviation, TickType_t *pxAbsoluteDeviation)
{
    if(pxRelativeDeviation != NULL)
    {
        *pxRelativeDeviation = g_xRelativeMaxDeviation;
    }
    if(pxAbsoluteDeviation != NULL)
    {
        *pxAbsoluteDeviation = g_xAbsoluteMaxDeviation;
    }
}

/**
 * @brief 실행 횟수 조회
 */
void vGetExecutionCounts(uint32_t *pulRelativeCount, uint32_t *pulAbsoluteCount)
{
    if(pulRelativeCount != NULL) *pulRelativeCount = g_ulRelativeCount;
    if(pulAbsoluteCount != NULL) *pulAbsoluteCount = g_ulAbsoluteCount;
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. vTaskDelay(xTicksToDelay)
 *    - 동작: 호출 시점에서 xTicksToDelay 틱 후 Ready 상태로 전환
 *    - 특징: 실행 시간이 주기에 포함됨
 *    - 사용 예: 정확한 주기가 중요하지 않은 경우
 *    - 코드:
 *      vTaskDelay(pdMS_TO_TICKS(100));  // 100ms 후 Ready
 * 
 * 2. vTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement)
 *    - 동작: pxPreviousWakeTime + xTimeIncrement 시점에 Ready 상태
 *    - 특징: 실행 시간과 무관하게 정확한 주기 유지
 *    - 사용 예: 주기적 제어 루프, 센서 샘플링
 *    - 코드:
 *      TickType_t xLastWakeTime = xTaskGetTickCount();  // 초기화 필수!
 *      for(;;) {
 *          // 작업...
 *          vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
 *      }
 * 
 * 3. 실행 결과 예측 (10초 후, 100ms 주기, 10ms 실행시간)
 *    
 *    ┌─────────────────┬──────────────┬──────────────┐
 *    │ 항목            │ vTaskDelay   │vTaskDelayUntil│
 *    ├─────────────────┼──────────────┼──────────────┤
 *    │ 실제 주기       │ 110ms        │ 100ms        │
 *    │ 10초간 실행횟수 │ ~91회        │ 100회        │
 *    │ 최대 편차       │ ~10ms 이상   │ ~1ms 이하    │
 *    └─────────────────┴──────────────┴──────────────┘
 * 
 * 4. 언제 어떤 것을 사용할까?
 *    - vTaskDelay:
 *      · LED 깜빡임 같은 정확도가 중요하지 않은 경우
 *      · 단발성 대기
 *      · 연속 실행을 피하기 위한 간단한 딜레이
 *    
 *    - vTaskDelayUntil:
 *      · 모터 제어 루프 (정밀 타이밍 필수)
 *      · 센서 샘플링 (일정한 샘플 레이트)
 *      · PWM 생성 (정확한 주기)
 *      · 통신 프로토콜 타이밍
 * 
 * 5. 주의사항
 *    - vTaskDelayUntil 사용 시 xLastWakeTime 반드시 초기화
 *    - 실행 시간이 주기보다 길면 두 함수 모두 문제 발생
 *    - configTICK_RATE_HZ (1000Hz = 1ms 해상도) 확인 필요
 */
