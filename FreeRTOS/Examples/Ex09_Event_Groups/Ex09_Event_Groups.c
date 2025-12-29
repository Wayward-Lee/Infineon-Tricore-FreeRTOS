/**********************************************************************************************************************
 * @file    Ex09_Event_Groups.c
 * @brief   FreeRTOS Example 09: Event Groups Demo (Multi-Task Synchronization)
 * 
 * @description
 * 이 예제는 Event Groups를 사용한 다중 태스크 동기화를 학습합니다.
 * 여러 태스크가 모두 준비될 때까지 대기하는 패턴을 구현합니다.
 * 
 * @learning_objectives
 * - xEventGroupCreate()로 Event Group 생성
 * - xEventGroupSetBits(), xEventGroupWaitBits() 사용법
 * - xEventGroupSync()로 Rendezvous 패턴 구현
 * - Event Bits를 이용한 상태 플래그 관리
 * 
 * @synchronization_pattern
 *     
 *     ┌──────────────────────────────────────────────────────────────┐
 *     │              Event Group 동기화 패턴                         │
 *     ├──────────────────────────────────────────────────────────────┤
 *     │                                                              │
 *     │   초기화 태스크들              Main Task                     │
 *     │                                                              │
 *     │   Task A ─SetBit(0x01)─┐                                    │
 *     │                        ├─> Event Group ─WaitBits(0x07)─>    │
 *     │   Task B ─SetBit(0x02)─┤     [0x07]                         │
 *     │                        │                                    │
 *     │   Task C ─SetBit(0x04)─┘                                    │
 *     │                                                              │
 *     │   모든 비트(0x07)가 설정되면 Main Task 진행                  │
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
#include "event_groups.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* Event Bits 정의 */
#define EVENT_TASK_A_READY  (1 << 0)    /* 0x01 - Task A 준비 완료 */
#define EVENT_TASK_B_READY  (1 << 1)    /* 0x02 - Task B 준비 완료 */
#define EVENT_TASK_C_READY  (1 << 2)    /* 0x04 - Task C 준비 완료 */

#define EVENT_ALL_READY     (EVENT_TASK_A_READY | EVENT_TASK_B_READY | EVENT_TASK_C_READY)  /* 0x07 */

/* 상태 플래그 (추가 예시) */
#define EVENT_DATA_RECEIVED (1 << 3)    /* 0x08 - 데이터 수신 완료 */
#define EVENT_ERROR_FLAG    (1 << 4)    /* 0x10 - 에러 발생 */

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* Event Group 핸들 */
static EventGroupHandle_t xEventGroup = NULL;

/* 태스크 핸들 */
static TaskHandle_t xInitTaskAHandle = NULL;
static TaskHandle_t xInitTaskBHandle = NULL;
static TaskHandle_t xInitTaskCHandle = NULL;
static TaskHandle_t xMainTaskHandle = NULL;

/* 통계 */
static volatile uint32_t g_ulSyncCount = 0;  /* 동기화 완료 횟수 */

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvInitTaskA(void *pvParameters);
static void prvInitTaskB(void *pvParameters);
static void prvInitTaskC(void *pvParameters);
static void prvMainTask(void *pvParameters);
static void prvToggleLED(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief 초기화 Task A
 * 
 * 초기화 작업을 완료하고 해당 비트를 설정합니다.
 */
static void prvInitTaskA(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        /* 초기화 작업 시뮬레이션 (예: 센서 A 준비) */
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        /*---------------------------------------------------------------------
         * xEventGroupSetBits(): 이벤트 비트 설정
         * 
         * 파라미터:
         * - xEventGroup: Event Group 핸들
         * - uxBitsToSet: 설정할 비트들 (OR 마스크)
         * 
         * 동작:
         * - 지정된 비트들을 1로 설정
         * - 대기 중인 태스크들의 조건을 확인하여 해제
         * 
         * 반환값: 현재 Event Group 값 (모든 비트)
         *---------------------------------------------------------------------*/
        xEventGroupSetBits(xEventGroup, EVENT_TASK_A_READY);
        
        /* 다음 사이클까지 대기 */
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/**
 * @brief 초기화 Task B
 */
static void prvInitTaskB(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        /* 초기화 작업 시뮬레이션 (예: 통신 모듈 B 준비) */
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        xEventGroupSetBits(xEventGroup, EVENT_TASK_B_READY);
        
        vTaskDelay(pdMS_TO_TICKS(2500));
    }
}

/**
 * @brief 초기화 Task C
 */
static void prvInitTaskC(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        /* 초기화 작업 시뮬레이션 (예: 모터 C 준비) */
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        xEventGroupSetBits(xEventGroup, EVENT_TASK_C_READY);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * @brief Main Task - 모든 초기화 완료 대기
 * 
 * 모든 초기화 태스크가 준비될 때까지 대기한 후 메인 동작을 시작합니다.
 */
static void prvMainTask(void *pvParameters)
{
    (void)pvParameters;
    
    EventBits_t uxBits;
    
    for(;;)
    {
        /*---------------------------------------------------------------------
         * xEventGroupWaitBits(): 이벤트 비트 대기
         * 
         * 파라미터:
         * - xEventGroup: Event Group 핸들
         * - uxBitsToWaitFor: 대기할 비트들 (OR 마스크)
         * - xClearOnExit: pdTRUE면 대기 비트를 자동으로 클리어
         * - xWaitForAllBits: pdTRUE = 모든 비트 대기 (AND)
         *                    pdFALSE = 하나라도 설정되면 (OR)
         * - xTicksToWait: 타임아웃
         * 
         * 반환값: 현재 Event Group 값 (대기 완료 또는 타임아웃 시)
         *---------------------------------------------------------------------*/
        uxBits = xEventGroupWaitBits(
            xEventGroup,
            EVENT_ALL_READY,        /* 0x07 = A, B, C 모두 */
            pdTRUE,                 /* 완료 후 비트 클리어 */
            pdTRUE,                 /* 모든 비트 필요 (AND) */
            portMAX_DELAY           /* 무한 대기 */
        );
        
        /* 모든 비트가 설정되어 깨어남 */
        if((uxBits & EVENT_ALL_READY) == EVENT_ALL_READY)
        {
            g_ulSyncCount++;
            
            /* 모든 초기화 완료 - 메인 동작 시작 */
            prvToggleLED();
            
            /* 
             * 실제 프로젝트에서:
             * - 시스템 메인 루프 시작
             * - 모든 서브시스템 활성화
             * - 상태 보고
             */
        }
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 09 데모 시작 함수
 */
void Ex09_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /*-------------------------------------------------------------------------
     * xEventGroupCreate(): Event Group 생성
     * 
     * Event Group 구조:
     * - 8/24 비트 사용 가능 (configUSE_16_BIT_TICKS에 따라)
     *   16비트 틱: 8비트 사용 가능
     *   32비트 틱: 24비트 사용 가능 (상위 8비트는 내부 사용)
     * - 각 비트는 독립적인 이벤트/플래그 표현
     * 
     * 반환값:
     * - 성공: Event Group 핸들
     * - 실패: NULL
     *-------------------------------------------------------------------------*/
    xEventGroup = xEventGroupCreate();
    
    if(xEventGroup != NULL)
    {
        /* 초기화 태스크들 생성 */
        xTaskCreate(prvInitTaskA, "InitA", configMINIMAL_STACK_SIZE,
                    NULL, tskIDLE_PRIORITY + 1, &xInitTaskAHandle);
        
        xTaskCreate(prvInitTaskB, "InitB", configMINIMAL_STACK_SIZE,
                    NULL, tskIDLE_PRIORITY + 1, &xInitTaskBHandle);
        
        xTaskCreate(prvInitTaskC, "InitC", configMINIMAL_STACK_SIZE,
                    NULL, tskIDLE_PRIORITY + 1, &xInitTaskCHandle);
        
        /* Main 태스크 (더 높은 우선순위) */
        xTaskCreate(prvMainTask, "Main", configMINIMAL_STACK_SIZE,
                    NULL, tskIDLE_PRIORITY + 2, &xMainTaskHandle);
    }
}

/*********************************************************************************************************************/
/*---------------------------------------------Additional Examples---------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief Rendezvous 동기화 패턴
 * 
 * 모든 태스크가 특정 지점에서 만나서 동시에 진행하는 패턴입니다.
 * xEventGroupSync()를 사용합니다.
 */
void vRendezvousExample(void)
{
    EventBits_t uxReturn;
    EventBits_t uxAllSyncBits = (1 << 0) | (1 << 1) | (1 << 2);  /* 3개 태스크 */
    EventBits_t uxThisTaskBit = (1 << 0);  /* 현재 태스크의 비트 */
    
    /* 
     * 각 태스크는 자신의 비트와 전체 비트를 지정
     * 모든 태스크가 이 함수를 호출하면 동시에 진행
     */
    
    /*-------------------------------------------------------------------------
     * xEventGroupSync(): Rendezvous 동기화
     * 
     * 동작:
     * 1. uxBitsToSet의 비트들을 설정
     * 2. uxBitsToWaitFor의 모든 비트가 설정될 때까지 대기
     * 3. 조건 충족 시 uxBitsToWaitFor 비트들을 모두 클리어
     * 
     * 특징:
     * - 모든 참여 태스크가 동시에 깨어남
     * - 장벽(Barrier) 동기화에 적합
     *-------------------------------------------------------------------------*/
    uxReturn = xEventGroupSync(
        xEventGroup,
        uxThisTaskBit,      /* 내가 설정할 비트 */
        uxAllSyncBits,      /* 모두 설정되길 기다릴 비트들 */
        portMAX_DELAY
    );
    
    /* 모든 태스크가 이 지점에 도달한 후 동시에 진행 */
    (void)uxReturn;
}

/**
 * @brief 비트 클리어 예시
 */
void vClearEventBits(EventBits_t uxBitsToClear)
{
    /*
     * xEventGroupClearBits(): 특정 비트 클리어
     * 반환값: 클리어 전의 Event Group 값
     */
    xEventGroupClearBits(xEventGroup, uxBitsToClear);
}

/**
 * @brief 현재 비트 조회 예시
 */
EventBits_t xGetEventBits(void)
{
    /*
     * xEventGroupGetBits(): 현재 비트 조회 (블로킹 없음)
     */
    return xEventGroupGetBits(xEventGroup);
}

/**
 * @brief 통계 조회
 */
void vGetEventGroupStats(uint32_t *pulSyncCount)
{
    if(pulSyncCount != NULL) *pulSyncCount = g_ulSyncCount;
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. Event Group vs 다른 동기화 메커니즘
 *    ┌─────────────────┬──────────────────────────────────────────┐
 *    │ 메커니즘        │ 특징                                     │
 *    ├─────────────────┼──────────────────────────────────────────┤
 *    │ Binary Sema     │ 단일 이벤트, 1개 태스크 대기             │
 *    │ Counting Sema   │ 카운트 이벤트, N개 태스크 대기           │
 *    │ Queue           │ 데이터 전달, FIFO                        │
 *    │ Event Group     │ 다중 이벤트, AND/OR 조건, 다중 태스크    │
 *    └─────────────────┴──────────────────────────────────────────┘
 * 
 * 2. Event Bits 구성
 *    
 *    24(또는 8) 비트 사용 가능
 *    
 *    Bit 23 ... Bit 4   Bit 3         Bit 2         Bit 1         Bit 0
 *    [ 예약 또는 사용자 정의 ][ DATA_RCV ][ TASK_C ][ TASK_B ][ TASK_A ]
 * 
 * 3. 대기 조건
 *    
 *    AND 모드 (xWaitForAllBits = pdTRUE):
 *      WaitBits(0x07) → 0x07 설정 시 깨어남
 *      사용 예: 모든 초기화 완료 대기
 *    
 *    OR 모드 (xWaitForAllBits = pdFALSE):
 *      WaitBits(0x07) → 0x01 또는 0x02 또는 0x04 설정 시 깨어남
 *      사용 예: 여러 이벤트 중 하나라도 처리
 * 
 * 4. 주요 API
 *    ┌───────────────────────────┬───────────────────────────────────┐
 *    │ API                       │ 설명                              │
 *    ├───────────────────────────┼───────────────────────────────────┤
 *    │ xEventGroupCreate()       │ Event Group 생성                  │
 *    │ xEventGroupSetBits()      │ 비트 설정 (태스크용)              │
 *    │ xEventGroupSetBitsFromISR()│ 비트 설정 (ISR용)                │
 *    │ xEventGroupClearBits()    │ 비트 클리어                       │
 *    │ xEventGroupWaitBits()     │ 비트 대기 (AND/OR 조건)           │
 *    │ xEventGroupSync()         │ Rendezvous 동기화                 │
 *    │ xEventGroupGetBits()      │ 현재 비트 조회                    │
 *    │ vEventGroupDelete()       │ Event Group 삭제                  │
 *    └───────────────────────────┴───────────────────────────────────┘
 * 
 * 5. 사용 패턴
 *    
 *    패턴 1: 다중 초기화 대기
 *      - 여러 서브시스템 초기화 완료 후 메인 루프 시작
 *    
 *    패턴 2: 상태 플래그 관리
 *      - 시스템 상태를 비트로 표현
 *      - 에러 플래그, 준비 상태 등
 *    
 *    패턴 3: Rendezvous (Barrier)
 *      - 모든 태스크가 특정 지점에서 만남
 *      - 동시에 다음 단계 진행
 * 
 * 6. 이 예제의 동작
 *    - Task A: 1초 후 BIT0 설정
 *    - Task B: 1.5초 후 BIT1 설정
 *    - Task C: 2초 후 BIT2 설정
 *    - Main: 2초 후 (모두 설정) 깨어남
 */
