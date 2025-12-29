/**********************************************************************************************************************
 * @file    Ex11_LED_Pattern.c
 * @brief   FreeRTOS Example 11: Multi-LED Pattern Controller (Practical Demo)
 * 
 * @description
 * 이 예제는 여러 FreeRTOS 기능을 결합하여 LED 패턴 컨트롤러를 구현합니다.
 * 실제 임베디드 시스템에서 사용되는 "명령-처리" 아키텍처 패턴을 학습합니다.
 * 
 * @learning_objectives
 * - Queue를 통한 명령 전달
 * - Software Timer를 이용한 패턴 타이밍
 * - 상태 머신(State Machine) 구현
 * - 여러 FreeRTOS 기능의 조합
 * 
 * @architecture
 *     
 *     ┌──────────────────────────────────────────────────────────────┐
 *     │                LED Pattern Controller 아키텍처              │
 *     ├──────────────────────────────────────────────────────────────┤
 *     │                                                              │
 *     │  ┌─────────────┐    Queue     ┌──────────────────┐          │
 *     │  │  Commander  │─────────────>│  Pattern Driver  │          │
 *     │  │    Task     │   (명령)     │      Task        │          │
 *     │  └─────────────┘              └────────┬─────────┘          │
 *     │                                        │                    │
 *     │                                        ▼                    │
 *     │                              ┌───────────────────┐          │
 *     │                              │  Software Timer   │          │
 *     │                              │  (패턴 타이밍)    │          │
 *     │                              └─────────┬─────────┘          │
 *     │                                        │                    │
 *     │                                        ▼                    │
 *     │                              ┌───────────────────┐          │
 *     │                              │       LED         │          │
 *     │                              │   (P22.5 등)      │          │
 *     │                              └───────────────────┘          │
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
#include "queue.h"
#include "timers.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* 명령 큐 설정 */
#define COMMAND_QUEUE_LENGTH    5

/*********************************************************************************************************************/
/*----------------------------------------------------Data Types-----------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief LED 패턴 종류
 */
typedef enum
{
    PATTERN_OFF = 0,        /* LED 끄기 */
    PATTERN_ON,             /* LED 켜기 */
    PATTERN_BLINK_SLOW,     /* 느린 깜빡임 (1초 주기) */
    PATTERN_BLINK_FAST,     /* 빠른 깜빡임 (200ms 주기) */
    PATTERN_HEARTBEAT,      /* 심장박동 패턴 (빠른 2회 + 긴 대기) */
    PATTERN_SOS             /* SOS 모스 코드 패턴 */
} LEDPattern_t;

/**
 * @brief 패턴 명령 구조체
 */
typedef struct
{
    LEDPattern_t ePattern;      /* 실행할 패턴 */
    uint32_t ulDuration;        /* 지속 시간 (0 = 무한) */
} PatternCommand_t;

/**
 * @brief Heartbeat 패턴 상태
 */
typedef enum
{
    HEARTBEAT_BEAT1_ON,
    HEARTBEAT_BEAT1_OFF,
    HEARTBEAT_BEAT2_ON,
    HEARTBEAT_BEAT2_OFF,
    HEARTBEAT_PAUSE
} HeartbeatState_t;

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* 큐 및 타이머 핸들 */
static QueueHandle_t xCommandQueue = NULL;
static TimerHandle_t xPatternTimer = NULL;

/* 태스크 핸들 */
static TaskHandle_t xCommanderTaskHandle = NULL;
static TaskHandle_t xDriverTaskHandle = NULL;

/* 현재 패턴 상태 */
static volatile LEDPattern_t g_eCurrentPattern = PATTERN_OFF;
static volatile HeartbeatState_t g_eHeartbeatState = HEARTBEAT_BEAT1_ON;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvCommanderTask(void *pvParameters);
static void prvPatternDriverTask(void *pvParameters);
static void prvPatternTimerCallback(TimerHandle_t xTimer);
static void prvSetLED(BaseType_t xState);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvSetLED(BaseType_t xState)
{
    if(xState)
    {
        IfxPort_setPinLow(LED_PORT, LED_PIN);   /* Low-active LED */
    }
    else
    {
        IfxPort_setPinHigh(LED_PORT, LED_PIN);
    }
}

/**
 * @brief 패턴 타이머 콜백
 * 
 * 현재 패턴에 따라 LED 상태를 변경합니다.
 */
static void prvPatternTimerCallback(TimerHandle_t xTimer)
{
    static BaseType_t xLEDState = pdFALSE;
    (void)xTimer;
    
    switch(g_eCurrentPattern)
    {
        case PATTERN_BLINK_SLOW:
        case PATTERN_BLINK_FAST:
            /* 단순 토글 */
            xLEDState = !xLEDState;
            prvSetLED(xLEDState);
            break;
            
        case PATTERN_HEARTBEAT:
            /* Heartbeat 상태 머신 */
            switch(g_eHeartbeatState)
            {
                case HEARTBEAT_BEAT1_ON:
                    prvSetLED(pdTRUE);
                    g_eHeartbeatState = HEARTBEAT_BEAT1_OFF;
                    xTimerChangePeriod(xPatternTimer, pdMS_TO_TICKS(100), 0);
                    break;
                    
                case HEARTBEAT_BEAT1_OFF:
                    prvSetLED(pdFALSE);
                    g_eHeartbeatState = HEARTBEAT_BEAT2_ON;
                    xTimerChangePeriod(xPatternTimer, pdMS_TO_TICKS(100), 0);
                    break;
                    
                case HEARTBEAT_BEAT2_ON:
                    prvSetLED(pdTRUE);
                    g_eHeartbeatState = HEARTBEAT_BEAT2_OFF;
                    xTimerChangePeriod(xPatternTimer, pdMS_TO_TICKS(100), 0);
                    break;
                    
                case HEARTBEAT_BEAT2_OFF:
                    prvSetLED(pdFALSE);
                    g_eHeartbeatState = HEARTBEAT_PAUSE;
                    xTimerChangePeriod(xPatternTimer, pdMS_TO_TICKS(600), 0);
                    break;
                    
                case HEARTBEAT_PAUSE:
                    g_eHeartbeatState = HEARTBEAT_BEAT1_ON;
                    xTimerChangePeriod(xPatternTimer, pdMS_TO_TICKS(100), 0);
                    break;
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Commander Task - 패턴 명령 생성
 * 
 * 주기적으로 패턴을 변경하는 명령을 전송합니다.
 * 실제 시스템에서는 버튼 입력이나 통신 명령으로 대체됩니다.
 */
static void prvCommanderTask(void *pvParameters)
{
    (void)pvParameters;
    
    PatternCommand_t xCommand;
    uint32_t ulPatternIndex = 0;
    
    /* 패턴 시퀀스 */
    LEDPattern_t aePatternSequence[] = {
        PATTERN_BLINK_SLOW,
        PATTERN_BLINK_FAST,
        PATTERN_HEARTBEAT,
        PATTERN_ON,
        PATTERN_OFF
    };
    
    for(;;)
    {
        /* 다음 패턴 선택 */
        xCommand.ePattern = aePatternSequence[ulPatternIndex];
        xCommand.ulDuration = 0;  /* 무한 */
        
        /* 명령 전송 */
        xQueueSend(xCommandQueue, &xCommand, 0);
        
        /* 다음 패턴 */
        ulPatternIndex = (ulPatternIndex + 1) % (sizeof(aePatternSequence) / sizeof(aePatternSequence[0]));
        
        /* 5초마다 패턴 변경 */
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief Pattern Driver Task - 패턴 실행
 * 
 * 명령을 받아 해당 패턴을 실행합니다.
 */
static void prvPatternDriverTask(void *pvParameters)
{
    (void)pvParameters;
    
    PatternCommand_t xReceivedCommand;
    TickType_t xTimerPeriod;
    
    for(;;)
    {
        /* 명령 대기 */
        if(xQueueReceive(xCommandQueue, &xReceivedCommand, portMAX_DELAY) == pdPASS)
        {
            /* 현재 타이머 정지 */
            xTimerStop(xPatternTimer, 0);
            
            /* 패턴 저장 */
            g_eCurrentPattern = xReceivedCommand.ePattern;
            g_eHeartbeatState = HEARTBEAT_BEAT1_ON;  /* Heartbeat 초기화 */
            
            /* 패턴별 처리 */
            switch(xReceivedCommand.ePattern)
            {
                case PATTERN_OFF:
                    prvSetLED(pdFALSE);
                    break;
                    
                case PATTERN_ON:
                    prvSetLED(pdTRUE);
                    break;
                    
                case PATTERN_BLINK_SLOW:
                    xTimerPeriod = pdMS_TO_TICKS(500);  /* 500ms on/off */
                    xTimerChangePeriod(xPatternTimer, xTimerPeriod, 0);
                    xTimerStart(xPatternTimer, 0);
                    break;
                    
                case PATTERN_BLINK_FAST:
                    xTimerPeriod = pdMS_TO_TICKS(100);  /* 100ms on/off */
                    xTimerChangePeriod(xPatternTimer, xTimerPeriod, 0);
                    xTimerStart(xPatternTimer, 0);
                    break;
                    
                case PATTERN_HEARTBEAT:
                    xTimerPeriod = pdMS_TO_TICKS(100);
                    xTimerChangePeriod(xPatternTimer, xTimerPeriod, 0);
                    xTimerStart(xPatternTimer, 0);
                    break;
                    
                default:
                    break;
            }
        }
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 11 데모 시작 함수
 */
void Ex11_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    prvSetLED(pdFALSE);
    
    /* 명령 큐 생성 */
    xCommandQueue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(PatternCommand_t));
    
    /* 패턴 타이머 생성 */
    xPatternTimer = xTimerCreate(
        "Pattern",
        pdMS_TO_TICKS(500),     /* 초기 주기 */
        pdTRUE,                 /* Auto-reload */
        NULL,
        prvPatternTimerCallback
    );
    
    if(xCommandQueue != NULL && xPatternTimer != NULL)
    {
        /* Pattern Driver Task 생성 */
        xTaskCreate(prvPatternDriverTask,
                    "Driver",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    tskIDLE_PRIORITY + 2,
                    &xDriverTaskHandle);
        
        /* Commander Task 생성 */
        xTaskCreate(prvCommanderTask,
                    "Commander",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    tskIDLE_PRIORITY + 1,
                    &xCommanderTaskHandle);
    }
}

/*********************************************************************************************************************/
/*-----------------------------------------External Pattern Command API----------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 외부에서 패턴 변경 요청
 */
BaseType_t xSetLEDPattern(LEDPattern_t ePattern)
{
    PatternCommand_t xCommand;
    
    if(xCommandQueue == NULL)
    {
        return pdFAIL;
    }
    
    xCommand.ePattern = ePattern;
    xCommand.ulDuration = 0;
    
    return xQueueSend(xCommandQueue, &xCommand, 0);
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. 명령-처리 아키텍처 (Command-Handler)
 *    
 *    ┌──────────┐     ┌───────┐     ┌──────────┐
 *    │ Commander│────>│ Queue │────>│ Handler  │
 *    │ (명령자) │     │       │     │ (처리자) │
 *    └──────────┘     └───────┘     └──────────┘
 *    
 *    장점:
 *    - 느슨한 결합 (Loose Coupling)
 *    - 명령 버퍼링
 *    - 우선순위 분리
 * 
 * 2. 상태 머신 (State Machine) 패턴
 *    
 *    Heartbeat 예시:
 *    BEAT1_ON → BEAT1_OFF → BEAT2_ON → BEAT2_OFF → PAUSE → (반복)
 *    
 *    상태 전이 시 타이머 주기도 변경
 * 
 * 3. 결합된 FreeRTOS 기능들
 *    - Queue: 태스크 간 명령 전달
 *    - Software Timer: 패턴 타이밍 제어
 *    - Task: 명령 생성과 처리 분리
 * 
 * 4. 확장 방법
 *    - 다중 LED 제어 (LED 배열)
 *    - 밝기 제어 (PWM 추가)
 *    - 외부 명령 수신 (UART, CAN)
 *    - 지속 시간 타이머 추가
 */
