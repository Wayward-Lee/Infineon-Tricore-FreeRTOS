/**********************************************************************************************************************
 * @file    Ex08_Software_Timer.c
 * @brief   FreeRTOS Example 08: Software Timer Demo (One-Shot and Auto-Reload)
 * 
 * @description
 * 이 예제는 FreeRTOS Software Timer의 사용법을 학습합니다.
 * 태스크 없이 주기적 또는 일회성 콜백을 실행하는 방법을 배웁니다.
 * 
 * @learning_objectives
 * - xTimerCreate()로 Software Timer 생성
 * - One-Shot vs Auto-Reload 타이머 차이
 * - 타이머 시작/정지/리셋/주기 변경
 * - Timer Service Task의 역할 이해
 * 
 * @timer_types
 *     
 *     ┌──────────────────────────────────────────────────────────────┐
 *     │                Software Timer 동작 방식                      │
 *     ├──────────────────────────────────────────────────────────────┤
 *     │                                                              │
 *     │  One-Shot Timer:                                            │
 *     │    Start ──[Period]──> Callback ──> 정지                    │
 *     │                                                              │
 *     │  Auto-Reload Timer:                                         │
 *     │    Start ──[Period]──> Callback ──[Period]──> Callback ──>  │
 *     │          (반복 실행)                                        │
 *     │                                                              │
 *     │  주의: 콜백은 Timer Service Task에서 실행됨                  │
 *     │        → 콜백에서 블로킹 API 사용 금지!                      │
 *     │                                                              │
 *     └──────────────────────────────────────────────────────────────┘
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
#include "timers.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* 타이머 주기 */
#define ONESHOT_PERIOD_MS   5000    /* One-Shot: 5초 후 한 번 실행 */
#define AUTORELOAD_PERIOD_MS 1000   /* Auto-Reload: 1초마다 실행 */

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* 타이머 핸들 */
static TimerHandle_t xOneShotTimer = NULL;
static TimerHandle_t xAutoReloadTimer = NULL;

/* 통계 */
static volatile uint32_t g_ulOneShotCount = 0;      /* One-Shot 콜백 횟수 */
static volatile uint32_t g_ulAutoReloadCount = 0;   /* Auto-Reload 콜백 횟수 */

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvOneShotTimerCallback(TimerHandle_t xTimer);
static void prvAutoReloadTimerCallback(TimerHandle_t xTimer);
static void prvToggleLED(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief One-Shot Timer 콜백 함수
 * 
 * 타이머 만료 시 한 번만 호출됩니다.
 * 다시 실행하려면 xTimerStart() 또는 xTimerReset()을 호출해야 합니다.
 * 
 * @param xTimer 만료된 타이머의 핸들
 */
static void prvOneShotTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    
    g_ulOneShotCount++;
    
    /*-------------------------------------------------------------------------
     * 콜백 함수 주의사항:
     * 
     * 1. Timer Service Task 컨텍스트에서 실행됨
     * 2. 블로킹 API 사용 금지 (vTaskDelay, xQueueReceive 등)
     * 3. 가능한 빠르게 반환해야 함
     * 4. 긴 처리가 필요하면 태스크에 시그널 전송
     *-------------------------------------------------------------------------*/
    
    /* LED 토글 - One-Shot 만료 표시 */
    prvToggleLED();
    
    /* 
     * One-Shot 타이머 특성:
     * - 이 콜백 후 타이머는 Dormant 상태
     * - 다시 실행하려면 명시적으로 시작해야 함
     */
}

/**
 * @brief Auto-Reload Timer 콜백 함수
 * 
 * 주기적으로 자동 호출됩니다.
 * xTimerStop()을 호출할 때까지 계속 실행됩니다.
 * 
 * @param xTimer 만료된 타이머의 핸들
 */
static void prvAutoReloadTimerCallback(TimerHandle_t xTimer)
{
    /* 타이머 ID 조회 예시 */
    void *pvTimerID = pvTimerGetTimerID(xTimer);
    (void)pvTimerID;
    
    g_ulAutoReloadCount++;
    
    /* LED 토글 - Auto-Reload 주기적 실행 표시 */
    prvToggleLED();
    
    /* 
     * Auto-Reload 타이머 특성:
     * - 콜백 완료 후 자동으로 다시 시작됨
     * - xTimerStop() 전까지 무한 반복
     */
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 08 데모 시작 함수
 */
void Ex08_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /*-------------------------------------------------------------------------
     * xTimerCreate(): Software Timer 생성
     * 
     * 파라미터:
     * - pcTimerName: 디버깅용 타이머 이름
     * - xTimerPeriodInTicks: 타이머 주기 (틱 단위)
     * - xAutoReload: pdTRUE = Auto-Reload, pdFALSE = One-Shot
     * - pvTimerID: 콜백에서 조회 가능한 식별자
     * - pxCallbackFunction: 만료 시 호출될 함수
     * 
     * 반환값:
     * - 성공: Timer 핸들
     * - 실패: NULL (메모리 부족)
     *-------------------------------------------------------------------------*/
    
    /* One-Shot Timer 생성 (5초 후 한 번만 실행) */
    xOneShotTimer = xTimerCreate(
        "OneShot",                              /* 타이머 이름 */
        pdMS_TO_TICKS(ONESHOT_PERIOD_MS),       /* 5000ms = 5초 */
        pdFALSE,                                /* One-Shot (자동 반복 안함) */
        (void *)1,                              /* Timer ID */
        prvOneShotTimerCallback                 /* 콜백 함수 */
    );
    
    /* Auto-Reload Timer 생성 (1초마다 반복 실행) */
    xAutoReloadTimer = xTimerCreate(
        "AutoReload",
        pdMS_TO_TICKS(AUTORELOAD_PERIOD_MS),    /* 1000ms = 1초 */
        pdTRUE,                                 /* Auto-Reload (자동 반복) */
        (void *)2,
        prvAutoReloadTimerCallback
    );
    
    if(xOneShotTimer != NULL && xAutoReloadTimer != NULL)
    {
        /*---------------------------------------------------------------------
         * xTimerStart(): 타이머 시작
         * 
         * 파라미터:
         * - xTimer: 타이머 핸들
         * - xTicksToWait: 명령 큐 대기 시간
         * 
         * 동작:
         * - Timer Service Task에 시작 명령 전송
         * - 실제 시작은 Timer Service Task에서 처리
         * 
         * 주의: ISR에서는 xTimerStartFromISR() 사용
         *---------------------------------------------------------------------*/
        xTimerStart(xOneShotTimer, 0);
        xTimerStart(xAutoReloadTimer, 0);
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------Timer Control Examples-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 타이머 정지
 */
void vStopTimers(void)
{
    if(xOneShotTimer != NULL)
    {
        /* 
         * xTimerStop(): 타이머 정지
         * - 대기 중인 콜백 취소
         * - Dormant 상태로 전환
         */
        xTimerStop(xOneShotTimer, 0);
    }
    
    if(xAutoReloadTimer != NULL)
    {
        xTimerStop(xAutoReloadTimer, 0);
    }
}

/**
 * @brief 타이머 리셋
 * 
 * 타이머를 처음부터 다시 시작합니다.
 * One-Shot 타이머의 경우 재시작에도 사용됩니다.
 */
void vResetTimers(void)
{
    if(xOneShotTimer != NULL)
    {
        /*
         * xTimerReset(): 타이머 리셋 및 재시작
         * - 정지 상태면 시작
         * - 실행 중이면 주기 처음부터 다시 카운트
         */
        xTimerReset(xOneShotTimer, 0);
    }
    
    if(xAutoReloadTimer != NULL)
    {
        xTimerReset(xAutoReloadTimer, 0);
    }
}

/**
 * @brief 타이머 주기 변경
 * 
 * @param ulNewPeriodMs 새로운 주기 (밀리초)
 */
void vChangeAutoReloadPeriod(uint32_t ulNewPeriodMs)
{
    if(xAutoReloadTimer != NULL)
    {
        /*
         * xTimerChangePeriod(): 타이머 주기 변경
         * - 새 주기 적용 및 타이머 재시작
         * - 정지 상태였으면 시작됨
         */
        xTimerChangePeriod(xAutoReloadTimer, 
                           pdMS_TO_TICKS(ulNewPeriodMs), 
                           0);
    }
}

/**
 * @brief 타이머 통계 조회
 */
void vGetTimerStats(uint32_t *pulOneShotCount, uint32_t *pulAutoReloadCount)
{
    if(pulOneShotCount != NULL) *pulOneShotCount = g_ulOneShotCount;
    if(pulAutoReloadCount != NULL) *pulAutoReloadCount = g_ulAutoReloadCount;
}

/**
 * @brief 타이머 활성 상태 확인
 */
BaseType_t xIsTimerActive(TimerHandle_t xTimer)
{
    /*
     * xTimerIsTimerActive(): 타이머 실행 중 여부
     * - pdTRUE: 실행 중 (Active)
     * - pdFALSE: 정지 상태 (Dormant)
     */
    return xTimerIsTimerActive(xTimer);
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. Software Timer vs Hardware Timer
 *    ┌───────────────────┬─────────────────┬────────────────────┐
 *    │ 특성              │ Software Timer  │ Hardware Timer     │
 *    ├───────────────────┼─────────────────┼────────────────────┤
 *    │ 리소스            │ RAM만 사용      │ 하드웨어 필요      │
 *    │ 개수 제한         │ 메모리만큼      │ 하드웨어 개수      │
 *    │ 정밀도            │ Tick 해상도     │ 하드웨어 클럭      │
 *    │ 최소 주기         │ 1 Tick (1ms)    │ 마이크로초 가능    │
 *    │ ISR 부하          │ 없음            │ 있음               │
 *    └───────────────────┴─────────────────┴────────────────────┘
 * 
 * 2. One-Shot vs Auto-Reload
 *    
 *    One-Shot (pdFALSE):
 *    ┌──────┐     ┌──────┐
 *    │Start │────>│5초후 │────> 정지 (Dormant)
 *    └──────┘     │콜백  │
 *                 └──────┘
 *    
 *    Auto-Reload (pdTRUE):
 *    ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐
 *    │Start │────>│1초후 │────>│1초후 │────>│1초후 │────> ...
 *    └──────┘     │콜백  │     │콜백  │     │콜백  │
 *                 └──────┘     └──────┘     └──────┘
 * 
 * 3. Timer Service Task (Daemon Task)
 *    - 모든 Software Timer 콜백은 이 태스크에서 실행
 *    - 우선순위: configTIMER_TASK_PRIORITY (FreeRTOSConfig.h에서 9)
 *    - 명령 큐: configTIMER_QUEUE_LENGTH (5개)
 *    
 *    App Task ──[명령]──> Timer Queue ──> Timer Service Task ──> 콜백
 * 
 * 4. 주요 API
 *    ┌─────────────────────────┬─────────────────────────────────┐
 *    │ API                     │ 설명                            │
 *    ├─────────────────────────┼─────────────────────────────────┤
 *    │ xTimerCreate()          │ 타이머 생성                     │
 *    │ xTimerStart()           │ 타이머 시작                     │
 *    │ xTimerStop()            │ 타이머 정지                     │
 *    │ xTimerReset()           │ 타이머 리셋 (재시작)            │
 *    │ xTimerChangePeriod()    │ 주기 변경                       │
 *    │ xTimerIsTimerActive()   │ 실행 여부 확인                  │
 *    │ pvTimerGetTimerID()     │ 타이머 ID 조회                  │
 *    │ vTimerSetTimerID()      │ 타이머 ID 설정                  │
 *    │ pcTimerGetName()        │ 타이머 이름 조회                │
 *    └─────────────────────────┴─────────────────────────────────┘
 * 
 * 5. 콜백 함수 제약사항
 *    ❌ vTaskDelay() 사용 금지
 *    ❌ 블로킹 API 사용 금지 (xQueueReceive with timeout 등)
 *    ❌ 긴 연산 수행 금지 (다른 타이머 콜백 지연)
 *    ✅ xQueueSendFromISR() 계열 API 사용 가능
 *    ✅ 짧은 처리만 수행
 *    ✅ 복잡한 처리는 태스크에 위임
 * 
 * 6. 사용 예
 *    - LED 깜빡임 (태스크 없이)
 *    - 주기적 센서 폴링 트리거
 *    - 타임아웃 감시
 *    - 소프트웨어 watchdog
 *    - 디바운스 타이머
 */
