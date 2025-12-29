/**********************************************************************************************************************
 * @file    Ex12_Button_Debounce.c
 * @brief   FreeRTOS Example 12: Button Input with Debouncing (Practical Demo)
 * 
 * @description
 * 이 예제는 버튼 입력 처리와 디바운싱을 FreeRTOS로 구현합니다.
 * Software Timer를 이용한 디바운스와 Queue를 이용한 이벤트 전달 패턴을 학습합니다.
 * 
 * @learning_objectives
 * - 버튼 디바운싱의 필요성과 구현 방법
 * - Software Timer를 이용한 디바운스 타이머
 * - 버튼 이벤트(Press, Release, Long Press) 처리
 * - ISR에서 태스크로의 이벤트 전달
 * 
 * @debounce_concept
 *     
 *     ┌──────────────────────────────────────────────────────────────┐
 *     │                 버튼 바운싱과 디바운스                       │
 *     ├──────────────────────────────────────────────────────────────┤
 *     │                                                              │
 *     │  물리적 버튼 상태 (바운싱 발생):                             │
 *     │    ─────┐   ┌┐┌─┐  ┌────────────────                         │
 *     │         └───┘└┘ └──┘                                         │
 *     │         <──────────>                                         │
 *     │           바운싱 구간                                        │
 *     │                                                              │
 *     │  디바운스 처리 후:                                           │
 *     │    ─────┐              ┌────────────────                     │
 *     │         └──────────────┘                                     │
 *     │              깨끗한 신호                                     │
 *     │                                                              │
 *     │  방법: 상태 변화 감지 후 일정 시간(20~50ms) 대기             │
 *     │        안정된 상태일 때만 이벤트 발생                        │
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
/* LED 설정 */
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* 버튼 설정 (예시 핀 - 실제 하드웨어에 맞게 수정) */
#define BUTTON_PORT         &MODULE_P15
#define BUTTON_PIN          8

/* 디바운스 설정 */
#define DEBOUNCE_TIME_MS    30      /* 디바운스 시간 */
#define LONG_PRESS_TIME_MS  1000    /* 롱 프레스 판정 시간 */

/* 버튼 폴링 주기 */
#define BUTTON_SCAN_PERIOD_MS   10

/*********************************************************************************************************************/
/*----------------------------------------------------Data Types-----------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 버튼 이벤트 종류
 */
typedef enum
{
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_PRESSED,       /* 버튼 눌림 */
    BUTTON_EVENT_RELEASED,      /* 버튼 떼어짐 */
    BUTTON_EVENT_LONG_PRESS,    /* 롱 프레스 (1초 이상) */
    BUTTON_EVENT_CLICK,         /* 클릭 (짧은 누름 후 떼어짐) */
    BUTTON_EVENT_DOUBLE_CLICK   /* 더블 클릭 */
} ButtonEvent_t;

/**
 * @brief 디바운스 상태
 */
typedef enum
{
    DEBOUNCE_IDLE,              /* 대기 상태 */
    DEBOUNCE_WAIT_STABLE        /* 안정화 대기 중 */
} DebounceState_t;

/**
 * @brief 버튼 상태 구조체
 */
typedef struct
{
    BaseType_t xCurrentState;       /* 현재 읽은 상태 */
    BaseType_t xLastStableState;    /* 마지막 안정 상태 */
    DebounceState_t eDebounceState; /* 디바운스 상태 */
    TickType_t xLastChangeTime;     /* 마지막 상태 변화 시간 */
    TickType_t xPressStartTime;     /* 눌림 시작 시간 */
    BaseType_t xLongPressReported;  /* 롱 프레스 이미 보고됨 */
} ButtonState_t;

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* 이벤트 큐 */
static QueueHandle_t xButtonEventQueue = NULL;

/* 태스크 핸들 */
static TaskHandle_t xButtonScanTaskHandle = NULL;
static TaskHandle_t xButtonHandlerTaskHandle = NULL;

/* 버튼 상태 */
static ButtonState_t g_xButtonState = {0};

/* 통계 */
static volatile uint32_t g_ulPressCount = 0;
static volatile uint32_t g_ulLongPressCount = 0;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvButtonScanTask(void *pvParameters);
static void prvButtonHandlerTask(void *pvParameters);
static BaseType_t prvReadButtonRaw(void);
static void prvProcessButtonDebounce(void);
static void prvSetLED(BaseType_t xState);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvSetLED(BaseType_t xState)
{
    if(xState)
    {
        IfxPort_setPinLow(LED_PORT, LED_PIN);
    }
    else
    {
        IfxPort_setPinHigh(LED_PORT, LED_PIN);
    }
}

/**
 * @brief 버튼 원시 상태 읽기
 * 
 * @return pdTRUE: 눌림, pdFALSE: 안 눌림
 */
static BaseType_t prvReadButtonRaw(void)
{
    /* 
     * 버튼이 GND로 풀다운되어 있다고 가정
     * 눌리면 HIGH, 안 눌리면 LOW
     * (실제 회로에 맞게 수정 필요)
     */
    
    /* 시뮬레이션: 실제로는 IfxPort_getPinState() 사용 */
    /* return IfxPort_getPinState(BUTTON_PORT, BUTTON_PIN) ? pdTRUE : pdFALSE; */
    
    /* 데모용: 주기적으로 가상 버튼 누름 시뮬레이션 */
    static uint32_t ulSimCounter = 0;
    ulSimCounter++;
    
    /* 매 5초마다 1초간 누름 시뮬레이션 */
    if((ulSimCounter % 500) >= 400)
    {
        return pdTRUE;
    }
    return pdFALSE;
}

/**
 * @brief 버튼 디바운스 처리
 * 
 * 상태 머신 기반 디바운스 알고리즘:
 * 1. 상태 변화 감지
 * 2. DEBOUNCE_TIME_MS 동안 안정화 대기
 * 3. 안정된 상태면 이벤트 발생
 */
static void prvProcessButtonDebounce(void)
{
    TickType_t xCurrentTime = xTaskGetTickCount();
    BaseType_t xRawState = prvReadButtonRaw();
    ButtonEvent_t eEvent = BUTTON_EVENT_NONE;
    
    g_xButtonState.xCurrentState = xRawState;
    
    switch(g_xButtonState.eDebounceState)
    {
        case DEBOUNCE_IDLE:
            /* 상태 변화 감지 */
            if(xRawState != g_xButtonState.xLastStableState)
            {
                g_xButtonState.xLastChangeTime = xCurrentTime;
                g_xButtonState.eDebounceState = DEBOUNCE_WAIT_STABLE;
            }
            else
            {
                /* 눌린 상태에서 롱 프레스 체크 */
                if(g_xButtonState.xLastStableState == pdTRUE && 
                   !g_xButtonState.xLongPressReported)
                {
                    if((xCurrentTime - g_xButtonState.xPressStartTime) >= 
                       pdMS_TO_TICKS(LONG_PRESS_TIME_MS))
                    {
                        eEvent = BUTTON_EVENT_LONG_PRESS;
                        g_xButtonState.xLongPressReported = pdTRUE;
                        g_ulLongPressCount++;
                    }
                }
            }
            break;
            
        case DEBOUNCE_WAIT_STABLE:
            /* 디바운스 시간 경과 확인 */
            if((xCurrentTime - g_xButtonState.xLastChangeTime) >= 
               pdMS_TO_TICKS(DEBOUNCE_TIME_MS))
            {
                /* 상태가 안정되었는지 확인 */
                if(xRawState != g_xButtonState.xLastStableState)
                {
                    /* 상태 변화 확정 */
                    g_xButtonState.xLastStableState = xRawState;
                    
                    if(xRawState == pdTRUE)
                    {
                        /* 버튼 눌림 */
                        eEvent = BUTTON_EVENT_PRESSED;
                        g_xButtonState.xPressStartTime = xCurrentTime;
                        g_xButtonState.xLongPressReported = pdFALSE;
                        g_ulPressCount++;
                    }
                    else
                    {
                        /* 버튼 떼어짐 */
                        eEvent = BUTTON_EVENT_RELEASED;
                    }
                }
                g_xButtonState.eDebounceState = DEBOUNCE_IDLE;
            }
            else if(xRawState == g_xButtonState.xLastStableState)
            {
                /* 다시 원래 상태로 돌아감 - 노이즈로 판단 */
                g_xButtonState.eDebounceState = DEBOUNCE_IDLE;
            }
            break;
    }
    
    /* 이벤트를 큐로 전송 */
    if(eEvent != BUTTON_EVENT_NONE && xButtonEventQueue != NULL)
    {
        xQueueSend(xButtonEventQueue, &eEvent, 0);
    }
}

/**
 * @brief Button Scan Task - 주기적 버튼 폴링
 */
static void prvButtonScanTask(void *pvParameters)
{
    (void)pvParameters;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        /* 버튼 디바운스 처리 */
        prvProcessButtonDebounce();
        
        /* 정확한 스캔 주기 유지 */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(BUTTON_SCAN_PERIOD_MS));
    }
}

/**
 * @brief Button Handler Task - 버튼 이벤트 처리
 */
static void prvButtonHandlerTask(void *pvParameters)
{
    (void)pvParameters;
    
    ButtonEvent_t eReceivedEvent;
    static BaseType_t xLEDState = pdFALSE;
    
    for(;;)
    {
        /* 이벤트 대기 */
        if(xQueueReceive(xButtonEventQueue, &eReceivedEvent, portMAX_DELAY) == pdPASS)
        {
            switch(eReceivedEvent)
            {
                case BUTTON_EVENT_PRESSED:
                    /* 눌렸을 때: 아무 동작 안함 (Release에서 처리) */
                    break;
                    
                case BUTTON_EVENT_RELEASED:
                    /* 짧은 클릭: LED 토글 */
                    if(!g_xButtonState.xLongPressReported)
                    {
                        xLEDState = !xLEDState;
                        prvSetLED(xLEDState);
                    }
                    break;
                    
                case BUTTON_EVENT_LONG_PRESS:
                    /* 롱 프레스: LED 빠르게 깜빡임 (피드백) */
                    for(int i = 0; i < 6; i++)
                    {
                        prvSetLED(i & 1);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    prvSetLED(xLEDState);  /* 원래 상태로 복구 */
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
 * @brief 예제 12 데모 시작 함수
 */
void Ex12_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    prvSetLED(pdFALSE);
    
    /* 버튼 핀 초기화 (입력) */
    /* IfxPort_setPinMode(BUTTON_PORT, BUTTON_PIN, IfxPort_Mode_inputPullDown); */
    
    /* 버튼 상태 초기화 */
    g_xButtonState.xCurrentState = pdFALSE;
    g_xButtonState.xLastStableState = pdFALSE;
    g_xButtonState.eDebounceState = DEBOUNCE_IDLE;
    g_xButtonState.xLongPressReported = pdFALSE;
    
    /* 이벤트 큐 생성 */
    xButtonEventQueue = xQueueCreate(10, sizeof(ButtonEvent_t));
    
    if(xButtonEventQueue != NULL)
    {
        /* Button Scan Task 생성 (높은 우선순위로 정확한 타이밍) */
        xTaskCreate(prvButtonScanTask,
                    "BtnScan",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    tskIDLE_PRIORITY + 3,
                    &xButtonScanTaskHandle);
        
        /* Button Handler Task 생성 */
        xTaskCreate(prvButtonHandlerTask,
                    "BtnHandler",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    tskIDLE_PRIORITY + 2,
                    &xButtonHandlerTaskHandle);
    }
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. 버튼 바운싱 문제
 *    - 기계식 스위치는 접점이 완전히 안정되기까지 진동
 *    - 수 ms ~ 수십 ms 동안 HIGH/LOW 반복
 *    - 디바운스 없이 읽으면 한 번 누름에 여러 이벤트 발생
 * 
 * 2. 디바운스 구현 방법
 *    
 *    방법 1: 지연 후 읽기 (단순)
 *      상태 변화 감지 → delay(30ms) → 상태 확인
 *    
 *    방법 2: 상태 머신 (이 예제)
 *      IDLE ─(변화)─> WAIT_STABLE ─(안정)─> IDLE + 이벤트
 *                        └──(원복)──> IDLE (노이즈)
 *    
 *    방법 3: 카운터 방식
 *      연속 N번 동일 상태 읽으면 확정
 * 
 * 3. 이벤트 종류
 *    
 *    ┌─────────────┬───────────────────────────────────┐
 *    │ 이벤트      │ 설명                              │
 *    ├─────────────┼───────────────────────────────────┤
 *    │ PRESSED     │ 눌림 확정 (디바운스 후)           │
 *    │ RELEASED    │ 떼어짐 확정                       │
 *    │ LONG_PRESS  │ 1초 이상 눌림                     │
 *    │ CLICK       │ 짧게 눌렀다 뗌                    │
 *    │ DOUBLE_CLICK│ 짧은 시간에 두 번 클릭            │
 *    └─────────────┴───────────────────────────────────┘
 * 
 * 4. 아키텍처
 *    
 *    ┌──────────┐   10ms   ┌───────────┐   Queue   ┌─────────┐
 *    │ 버튼 핀  │─────────>│ Scan Task │─────────>│ Handler │
 *    │ (GPIO)   │  폴링    │ (디바운스)│  이벤트  │  Task   │
 *    └──────────┘          └───────────┘          └─────────┘
 *    
 *    분리 장점:
 *    - Scan: 빠른 폴링에 집중
 *    - Handler: 이벤트 처리에 집중 (블로킹 가능)
 * 
 * 5. 주의사항
 *    - 폴링 주기 < 디바운스 시간/2
 *    - 롱 프레스는 누르고 있는 동안 한 번만 보고
 *    - 더블 클릭 구현 시 타임아웃 추가 필요
 */
