/**********************************************************************************************************************
 * @file    Ex10_Task_Notification.c
 * @brief   FreeRTOS Example 10: Task Notification Demo (Lightweight IPC)
 * 
 * @description
 * 이 예제는 Task Notification을 사용한 경량 IPC(Inter-Process Communication)를 학습합니다.
 * Semaphore나 Queue 대비 더 빠르고 메모리 효율적인 통신 방법입니다.
 * 
 * @learning_objectives
 * - xTaskNotifyGive()/ulTaskNotifyTake() - Binary Semaphore 대체
 * - xTaskNotify()/xTaskNotifyWait() - 값 전달 및 유연한 동작
 * - Task Notification의 장단점 이해
 * 
 * @comparison
 *     
 *     ┌──────────────────────────────────────────────────────────────┐
 *     │           Task Notification vs 전통적 IPC                   │
 *     ├──────────────────────────────────────────────────────────────┤
 *     │                                                              │
 *     │  전통적 방식 (Binary Semaphore):                            │
 *     │    Task A ──> Semaphore(별도 객체) ──> Task B               │
 *     │    - 별도 세마포어 생성 필요                                 │
 *     │    - 추가 RAM 사용                                          │
 *     │                                                              │
 *     │  Task Notification:                                         │
 *     │    Task A ──────────────────────────> Task B (직접)         │
 *     │    - 별도 객체 불필요                                       │
 *     │    - TCB 내장 값 사용                                       │
 *     │    - 더 빠름 (약 45% 향상)                                  │
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

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* 태스크 핸들 */
static TaskHandle_t xNotifierTaskHandle = NULL;
static TaskHandle_t xReceiverTaskHandle = NULL;
static TaskHandle_t xValueReceiverTaskHandle = NULL;

/* 통계 */
static volatile uint32_t g_ulNotifyCount = 0;
static volatile uint32_t g_ulReceivedCount = 0;
static volatile uint32_t g_ulLastReceivedValue = 0;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvNotifierTask(void *pvParameters);
static void prvReceiverTask(void *pvParameters);
static void prvValueReceiverTask(void *pvParameters);
static void prvToggleLED(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief Notifier Task - 알림 전송
 * 
 * Binary Semaphore의 Give와 유사한 동작을 xTaskNotifyGive()로 수행합니다.
 */
static void prvNotifierTask(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        /*---------------------------------------------------------------------
         * xTaskNotifyGive(): 태스크에 알림 전송 (카운트 증가)
         * 
         * 동작:
         * - 대상 태스크의 Notification Value를 1 증가
         * - 대상 태스크가 대기 중이면 깨움
         * 
         * Binary Semaphore의 xSemaphoreGive()와 유사하지만:
         * - 별도 세마포어 객체 불필요
         * - 직접 태스크를 지정
         * 
         * 반환값: 항상 pdPASS
         *---------------------------------------------------------------------*/
        if(xReceiverTaskHandle != NULL)
        {
            xTaskNotifyGive(xReceiverTaskHandle);
            g_ulNotifyCount++;
        }
    }
}

/**
 * @brief Receiver Task - 알림 수신 (Binary Semaphore 스타일)
 * 
 * Binary Semaphore의 Take와 유사한 동작을 ulTaskNotifyTake()로 수행합니다.
 */
static void prvReceiverTask(void *pvParameters)
{
    (void)pvParameters;
    
    uint32_t ulNotificationValue;
    
    for(;;)
    {
        /*---------------------------------------------------------------------
         * ulTaskNotifyTake(): 알림 대기 (카운트 감소)
         * 
         * 파라미터:
         * - xClearCountOnExit: 
         *     pdTRUE = 반환 시 값을 0으로 클리어 (Binary Sema 동작)
         *     pdFALSE = 반환 시 값을 1 감소 (Counting Sema 동작)
         * - xTicksToWait: 대기 시간
         * 
         * 반환값:
         * - 클리어/감소 전의 Notification Value
         * - 타임아웃 시 0
         * 
         * Binary Semaphore의 xSemaphoreTake()와 유사
         *---------------------------------------------------------------------*/
        ulNotificationValue = ulTaskNotifyTake(
            pdTRUE,             /* Binary 모드: 값을 0으로 클리어 */
            portMAX_DELAY       /* 무한 대기 */
        );
        
        if(ulNotificationValue > 0)
        {
            g_ulReceivedCount++;
            prvToggleLED();
        }
    }
}

/**
 * @brief Value Receiver Task - 값과 함께 알림 수신
 * 
 * xTaskNotify()와 xTaskNotifyWait()를 사용하여 값을 전달합니다.
 */
static void prvValueReceiverTask(void *pvParameters)
{
    (void)pvParameters;
    
    uint32_t ulNotificationValue;
    BaseType_t xResult;
    
    for(;;)
    {
        /*---------------------------------------------------------------------
         * xTaskNotifyWait(): 알림 대기 (더 유연한 인터페이스)
         * 
         * 파라미터:
         * - ulBitsToClearOnEntry: 대기 시작 시 클리어할 비트들
         * - ulBitsToClearOnExit: 반환 시 클리어할 비트들
         * - pulNotificationValue: 수신된 값 저장 위치
         * - xTicksToWait: 대기 시간
         * 
         * 반환값:
         * - pdPASS: 알림 수신 성공
         * - pdFAIL: 타임아웃
         * 
         * 특징:
         * - 32비트 값 전체 수신 가능
         * - 비트 마스크 연산 지원
         *---------------------------------------------------------------------*/
        xResult = xTaskNotifyWait(
            0x00,               /* 진입 시 클리어할 비트 없음 */
            ULONG_MAX,          /* 종료 시 모든 비트 클리어 */
            &ulNotificationValue,
            portMAX_DELAY
        );
        
        if(xResult == pdPASS)
        {
            g_ulLastReceivedValue = ulNotificationValue;
            
            /* 수신된 값에 따른 처리 */
            if(ulNotificationValue & 0x01)
            {
                /* 비트 0 설정됨 */
            }
            if(ulNotificationValue & 0x02)
            {
                /* 비트 1 설정됨 */
            }
        }
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 10 데모 시작 함수
 */
void Ex10_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /* 
     * Task Notification 특징:
     * - 생성할 객체 없음! (Semaphore, Queue 생성 불필요)
     * - 각 태스크는 TCB에 내장된 32비트 Notification Value 보유
     * - configTASK_NOTIFICATION_ARRAY_ENTRIES = 1 (기본)
     */
    
    /* Receiver Task 생성 (먼저 생성하여 핸들 확보) */
    xTaskCreate(prvReceiverTask,
                "Receiver",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                &xReceiverTaskHandle);
    
    /* Value Receiver Task 생성 */
    xTaskCreate(prvValueReceiverTask,
                "ValRecv",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 2,
                &xValueReceiverTaskHandle);
    
    /* Notifier Task 생성 */
    xTaskCreate(prvNotifierTask,
                "Notifier",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                &xNotifierTaskHandle);
}

/*********************************************************************************************************************/
/*--------------------------------------------Additional Examples----------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 값과 함께 알림 전송 예시
 * 
 * xTaskNotify()의 다양한 동작 모드를 보여줍니다.
 */
void vSendNotificationWithValue(void)
{
    if(xValueReceiverTaskHandle != NULL)
    {
        /*-----------------------------------------------------------------
         * xTaskNotify(): 값과 함께 알림 전송
         * 
         * 파라미터:
         * - xTaskToNotify: 대상 태스크 핸들
         * - ulValue: 전송할 값
         * - eAction: 동작 모드
         *     eNoAction: 단순 알림만 (값 변경 안함)
         *     eSetBits: 기존 값에 OR 연산
         *     eIncrement: 기존 값에 +1
         *     eSetValueWithOverwrite: 값 덮어쓰기
         *     eSetValueWithoutOverwrite: 이전 알림 처리 안됐으면 실패
         *-----------------------------------------------------------------*/
        
        /* 예시 1: 비트 설정 (Event Group과 유사) */
        xTaskNotify(xValueReceiverTaskHandle, 
                    0x01,                   /* 비트 0 설정 */
                    eSetBits);
        
        /* 예시 2: 값 덮어쓰기 */
        xTaskNotify(xValueReceiverTaskHandle,
                    42,                     /* 새 값 */
                    eSetValueWithOverwrite);
        
        /* 예시 3: 카운트 증가 (xTaskNotifyGive와 동일) */
        xTaskNotify(xValueReceiverTaskHandle,
                    0,                      /* 무시됨 */
                    eIncrement);
    }
}

/**
 * @brief ISR에서 알림 전송 예시
 */
void vISR_SendNotification(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(xReceiverTaskHandle != NULL)
    {
        /*
         * vTaskNotifyGiveFromISR(): ISR용 알림 전송
         */
        vTaskNotifyGiveFromISR(xReceiverTaskHandle, &xHigherPriorityTaskWoken);
        
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief 통계 조회
 */
void vGetNotificationStats(uint32_t *pulNotifyCount, uint32_t *pulReceivedCount)
{
    if(pulNotifyCount != NULL) *pulNotifyCount = g_ulNotifyCount;
    if(pulReceivedCount != NULL) *pulReceivedCount = g_ulReceivedCount;
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. Task Notification 장단점
 *    
 *    장점:
 *    ✅ 더 빠름 (Semaphore 대비 약 45% 속도 향상)
 *    ✅ RAM 절약 (별도 객체 생성 불필요)
 *    ✅ 간단한 사용법
 *    
 *    단점:
 *    ❌ 1:1 통신만 (여러 태스크에서 대기 불가)
 *    ❌ 송신 측이 수신 태스크 핸들 알아야 함
 *    ❌ 큐처럼 여러 값 버퍼링 불가
 * 
 * 2. 전통적 IPC 대체 패턴
 *    
 *    ┌─────────────────────┬─────────────────────────────────────┐
 *    │ 대체 대상           │ Task Notification API               │
 *    ├─────────────────────┼─────────────────────────────────────┤
 *    │ Binary Semaphore    │ xTaskNotifyGive()                   │
 *    │                     │ ulTaskNotifyTake(pdTRUE, ...)       │
 *    ├─────────────────────┼─────────────────────────────────────┤
 *    │ Counting Semaphore  │ xTaskNotifyGive()                   │
 *    │                     │ ulTaskNotifyTake(pdFALSE, ...)      │
 *    ├─────────────────────┼─────────────────────────────────────┤
 *    │ Event Group Bit     │ xTaskNotify(..., eSetBits)          │
 *    │                     │ xTaskNotifyWait()                   │
 *    ├─────────────────────┼─────────────────────────────────────┤
 *    │ Mailbox (1 값)      │ xTaskNotify(..., eSetValueXxx)      │
 *    │                     │ xTaskNotifyWait()                   │
 *    └─────────────────────┴─────────────────────────────────────┘
 * 
 * 3. eAction 동작 모드
 *    
 *    eNoAction:
 *      단순 알림만, 값 변경 없음
 *      
 *    eSetBits:
 *      NotificationValue |= ulValue
 *      Event Group의 SetBits와 유사
 *    
 *    eIncrement:
 *      NotificationValue++
 *      xTaskNotifyGive()와 동일
 *    
 *    eSetValueWithOverwrite:
 *      NotificationValue = ulValue
 *      항상 덮어씀
 *    
 *    eSetValueWithoutOverwrite:
 *      이전 값이 처리 안됐으면 pdFAIL 반환
 *      처리됐으면 NotificationValue = ulValue
 * 
 * 4. 언제 사용할까?
 *    
 *    Task Notification 사용:
 *    - 1:1 통신
 *    - 단순 이벤트/시그널
 *    - 메모리가 제한적인 환경
 *    - 최대 성능이 필요한 경우
 *    
 *    전통적 IPC 사용:
 *    - N:1 또는 1:N 통신
 *    - 데이터 버퍼링 필요
 *    - 여러 태스크가 동시 대기
 *    - 구조체 등 복잡한 데이터 전송
 * 
 * 5. 주요 API 요약
 *    ┌─────────────────────────────┬───────────────────────────────┐
 *    │ API                         │ 설명                          │
 *    ├─────────────────────────────┼───────────────────────────────┤
 *    │ xTaskNotifyGive()           │ 카운트 +1 (Give)              │
 *    │ ulTaskNotifyTake()          │ 카운트 대기/감소 (Take)       │
 *    │ xTaskNotify()               │ 유연한 알림 전송              │
 *    │ xTaskNotifyWait()           │ 유연한 알림 대기              │
 *    │ xTaskNotifyStateClear()     │ Pending 상태 클리어           │
 *    │ ulTaskNotifyValueClear()    │ 특정 비트 클리어              │
 *    └─────────────────────────────┴───────────────────────────────┘
 */
