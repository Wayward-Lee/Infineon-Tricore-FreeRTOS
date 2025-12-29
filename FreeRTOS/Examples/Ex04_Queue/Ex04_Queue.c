/**********************************************************************************************************************
 * @file    Ex04_Queue.c
 * @brief   FreeRTOS Example 04: Queue Communication Demo (Producer/Consumer Pattern)
 * 
 * @description
 * 이 예제는 FreeRTOS Queue를 사용한 태스크 간 데이터 통신을 학습합니다.
 * Producer-Consumer 패턴을 구현하여 Queue의 FIFO 동작과 블로킹 기능을 실습합니다.
 * 
 * @learning_objectives
 * - xQueueCreate()로 큐 생성
 * - xQueueSend()/xQueueReceive()로 데이터 송수신
 * - 큐 가득 참/비어있음 상태에서의 블로킹 동작 이해
 * - uxQueueMessagesWaiting()으로 큐 상태 조회
 * 
 * @architecture
 *     ┌──────────────┐         ┌───────────────┐         ┌──────────────┐
 *     │   Producer   │────────>│     Queue     │────────>│   Consumer   │
 *     │    Task      │  Send   │  (5 items)    │ Receive │    Task      │
 *     └──────────────┘         └───────────────┘         └──────────────┘
 *           │                         │                         │
 *      센서 데이터 생성           FIFO 버퍼              데이터 처리
 *      (200ms 주기)                                    LED로 표시
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

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* 큐 설정 */
#define QUEUE_LENGTH        5       /* 큐에 저장할 수 있는 최대 아이템 개수 */
#define QUEUE_ITEM_SIZE     sizeof(SensorData_t)

/* 태스크 주기 */
#define PRODUCER_PERIOD_MS  200     /* Producer: 200ms 마다 데이터 생성 */
#define CONSUMER_PERIOD_MS  500     /* Consumer: 500ms 마다 데이터 처리 */

/*********************************************************************************************************************/
/*----------------------------------------------------Data Types-----------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 센서 데이터 구조체
 * 
 * Queue를 통해 전송될 데이터의 형태를 정의합니다.
 * 구조체를 사용하면 여러 관련 데이터를 한 번에 전송할 수 있습니다.
 */
typedef struct
{
    uint32_t ulSensorId;        /* 센서 식별자 */
    uint32_t ulValue;           /* 측정 값 */
    TickType_t xTimestamp;      /* 측정 시간 (틱) */
} SensorData_t;

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* 큐 핸들 */
static QueueHandle_t xSensorQueue = NULL;

/* 태스크 핸들 */
static TaskHandle_t xProducerTaskHandle = NULL;
static TaskHandle_t xConsumerTaskHandle = NULL;

/* 통계 */
static volatile uint32_t g_ulSentCount = 0;      /* 전송된 데이터 수 */
static volatile uint32_t g_ulReceivedCount = 0;  /* 수신된 데이터 수 */
static volatile uint32_t g_ulQueueFullCount = 0; /* 큐 가득 참 횟수 */

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvProducerTask(void *pvParameters);
static void prvConsumerTask(void *pvParameters);
static void prvToggleLED(void);
static uint32_t prvSimulateSensorReading(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief 센서 값 시뮬레이션
 * 
 * 실제 프로젝트에서는 ADC 읽기 등으로 대체됩니다.
 */
static uint32_t prvSimulateSensorReading(void)
{
    static uint32_t ulSimValue = 0;
    ulSimValue = (ulSimValue + 17) % 256;  /* 간단한 패턴 생성 */
    return ulSimValue;
}

/**
 * @brief Producer Task - 데이터 생성 및 큐로 전송
 * 
 * 주기적으로 센서 데이터를 생성하고 큐로 전송합니다.
 * 큐가 가득 찬 경우 블로킹되어 소비자가 공간을 만들 때까지 대기합니다.
 */
static void prvProducerTask(void *pvParameters)
{
    (void)pvParameters;
    
    SensorData_t xDataToSend;
    BaseType_t xQueueStatus;
    UBaseType_t uxQueueSpaces;
    
    for(;;)
    {
        /* 센서 데이터 생성 */
        xDataToSend.ulSensorId = 1;
        xDataToSend.ulValue = prvSimulateSensorReading();
        xDataToSend.xTimestamp = xTaskGetTickCount();
        
        /* 큐의 남은 공간 확인 */
        uxQueueSpaces = uxQueueSpacesAvailable(xSensorQueue);
        
        /*---------------------------------------------------------------------
         * xQueueSend(): 큐에 데이터 전송 (Back에 추가)
         * 
         * 파라미터:
         * - xSensorQueue: 대상 큐
         * - &xDataToSend: 전송할 데이터 포인터 (값 복사됨)
         * - portMAX_DELAY: 무한 대기 (큐 공간 생길 때까지)
         *                  0: 즉시 반환 (공간 없으면 실패)
         *                  pdMS_TO_TICKS(100): 100ms 타임아웃
         * 
         * 반환값:
         * - pdPASS: 전송 성공
         * - errQUEUE_FULL: 타임아웃 내에 전송 실패
         *---------------------------------------------------------------------*/
        xQueueStatus = xQueueSend(xSensorQueue, &xDataToSend, portMAX_DELAY);
        
        if(xQueueStatus == pdPASS)
        {
            g_ulSentCount++;
        }
        else
        {
            /* 큐 가득 참 (portMAX_DELAY에서는 발생하지 않음) */
            g_ulQueueFullCount++;
        }
        
        /* Producer 주기 */
        vTaskDelay(pdMS_TO_TICKS(PRODUCER_PERIOD_MS));
    }
}

/**
 * @brief Consumer Task - 큐에서 데이터 수신 및 처리
 * 
 * 큐에서 데이터를 수신하고 처리합니다.
 * 큐가 비어있으면 블로킹되어 데이터가 도착할 때까지 대기합니다.
 */
static void prvConsumerTask(void *pvParameters)
{
    (void)pvParameters;
    
    SensorData_t xReceivedData;
    BaseType_t xQueueStatus;
    UBaseType_t uxMessagesWaiting;
    
    for(;;)
    {
        /* 큐에 대기 중인 메시지 개수 확인 */
        uxMessagesWaiting = uxQueueMessagesWaiting(xSensorQueue);
        (void)uxMessagesWaiting;  /* 디버거에서 확인용 */
        
        /*---------------------------------------------------------------------
         * xQueueReceive(): 큐에서 데이터 수신 (Front에서 제거)
         * 
         * 파라미터:
         * - xSensorQueue: 대상 큐
         * - &xReceivedData: 수신 데이터 저장 위치
         * - portMAX_DELAY: 무한 대기 (데이터 도착까지)
         * 
         * 반환값:
         * - pdPASS: 수신 성공
         * - pdFAIL: 타임아웃 (큐가 비어있고 시간 초과)
         *---------------------------------------------------------------------*/
        xQueueStatus = xQueueReceive(xSensorQueue, &xReceivedData, portMAX_DELAY);
        
        if(xQueueStatus == pdPASS)
        {
            g_ulReceivedCount++;
            
            /* 데이터 처리: 값의 크기에 따라 LED 토글 횟수 결정 */
            if(xReceivedData.ulValue > 128)
            {
                prvToggleLED();
            }
            
            /* 
             * 실제 프로젝트에서의 처리 예:
             * - 디스플레이 업데이트
             * - 제어 알고리즘 실행
             * - 로그 저장
             * - 다른 태스크로 가공된 데이터 전달
             */
        }
        
        /* Consumer 주기 (Producer보다 느림 - 큐에 데이터가 쌓임) */
        vTaskDelay(pdMS_TO_TICKS(CONSUMER_PERIOD_MS));
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 04 데모 시작 함수
 */
void Ex04_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /*-------------------------------------------------------------------------
     * xQueueCreate(): 큐 생성
     * 
     * 파라미터:
     * - QUEUE_LENGTH: 큐가 저장할 수 있는 최대 아이템 개수
     * - QUEUE_ITEM_SIZE: 각 아이템의 바이트 크기
     * 
     * 반환값:
     * - 성공: 큐 핸들 (QueueHandle_t)
     * - 실패: NULL (메모리 부족)
     * 
     * 주의: 큐는 값 복사(copy by value) 방식으로 동작
     *       포인터만 전달하려면 sizeof(void*) 사용
     *-------------------------------------------------------------------------*/
    xSensorQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
    
    if(xSensorQueue != NULL)
    {
        /* Producer Task 생성 (낮은 우선순위) */
        xTaskCreate(prvProducerTask,
                    "Producer",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    tskIDLE_PRIORITY + 1,
                    &xProducerTaskHandle);
        
        /* Consumer Task 생성 (높은 우선순위) */
        xTaskCreate(prvConsumerTask,
                    "Consumer",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    tskIDLE_PRIORITY + 2,
                    &xConsumerTaskHandle);
    }
}

/*********************************************************************************************************************/
/*-----------------------------------------------Additional Queue APIs-----------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 큐에 데이터 앞쪽에 삽입 (높은 우선순위 데이터)
 */
void vDemoQueueSendToFront(SensorData_t *pxData)
{
    if(xSensorQueue != NULL && pxData != NULL)
    {
        /* xQueueSendToFront(): 큐의 앞쪽(Front)에 데이터 삽입 */
        xQueueSendToFront(xSensorQueue, pxData, 0);
    }
}

/**
 * @brief 큐 데이터 복사 없이 확인 (Peek)
 */
BaseType_t xDemoQueuePeek(SensorData_t *pxData)
{
    if(xSensorQueue != NULL && pxData != NULL)
    {
        /* 
         * xQueuePeek(): 큐의 Front 데이터를 복사하지만 제거하지 않음
         * 여러 태스크가 동일한 데이터를 확인해야 할 때 유용
         */
        return xQueuePeek(xSensorQueue, pxData, 0);
    }
    return pdFAIL;
}

/**
 * @brief 큐 통계 조회
 */
void vGetQueueStats(uint32_t *pulSent, uint32_t *pulReceived, uint32_t *pulFullCount)
{
    if(pulSent != NULL) *pulSent = g_ulSentCount;
    if(pulReceived != NULL) *pulReceived = g_ulReceivedCount;
    if(pulFullCount != NULL) *pulFullCount = g_ulQueueFullCount;
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. Queue 기본 개념
 *    - FIFO(First In, First Out) 버퍼
 *    - 태스크 간 데이터 안전 전송 (Copy by Value)
 *    - 자동 블로킹 지원 (가득 참/비어있음 상태)
 * 
 * 2. 주요 API
 *    ┌─────────────────────┬─────────────────────────────────────┐
 *    │ API                 │ 설명                                │
 *    ├─────────────────────┼─────────────────────────────────────┤
 *    │ xQueueCreate()      │ 큐 생성 (길이, 아이템 크기)         │
 *    │ xQueueSend()        │ 큐 뒤쪽에 데이터 추가               │
 *    │ xQueueSendToFront() │ 큐 앞쪽에 데이터 추가 (우선순위 높음)│
 *    │ xQueueReceive()     │ 큐 앞에서 데이터 제거 및 수신       │
 *    │ xQueuePeek()        │ 데이터 확인만 (제거 안함)           │
 *    │ xQueueReset()       │ 큐 내용 모두 삭제                   │
 *    │ uxQueueMessagesWaiting()│ 대기 중인 메시지 개수 조회      │
 *    │ uxQueueSpacesAvailable()│ 남은 공간 개수 조회            │
 *    └─────────────────────┴─────────────────────────────────────┘
 * 
 * 3. 블로킹 동작
 *    - xQueueSend with portMAX_DELAY: 공간 생길 때까지 무한 대기
 *    - xQueueReceive with portMAX_DELAY: 데이터 올 때까지 무한 대기
 *    - 타임아웃 지정: pdMS_TO_TICKS(100) = 100ms 대기
 *    - 즉시 반환: 0 (대기 없이 즉시 성공/실패 반환)
 * 
 * 4. ISR에서 사용
 *    - xQueueSendFromISR() / xQueueReceiveFromISR() 사용
 *    - 블로킹 불가 (타임아웃 항상 0)
 * 
 * 5. 이 예제의 동작
 *    - Producer: 200ms마다 데이터 생성 (초당 5개)
 *    - Consumer: 500ms마다 데이터 처리 (초당 2개)
 *    - 큐 길이: 5개
 *    - 예상: 큐가 점점 차다가 가득 참 → Producer 블로킹
 * 
 * 6. 포인터 vs 값 복사
 *    값 복사 (권장):
 *    xQueueSend(xQueue, &structData, 0);  // 구조체 전체 복사
 *    
 *    포인터 전달 (주의 필요):
 *    void *pData = &structData;
 *    xQueueSend(xQueue, &pData, 0);  // 포인터만 복사
 *    // 주의: structData의 수명이 수신 측에서도 유효해야 함!
 */
