/**********************************************************************************************************************
 * @file    Ex02_Task_States.c
 * @brief   FreeRTOS Example 02: Task State Management Demo
 * 
 * @description
 * 이 예제는 FreeRTOS 태스크의 상태 전이(State Transition)를 학습합니다.
 * Ready, Running, Blocked, Suspended 상태 간의 전환을 실습합니다.
 * 
 * @learning_objectives
 * - 태스크의 4가지 상태 이해 (Ready, Running, Blocked, Suspended)
 * - vTaskSuspend(), vTaskResume() 사용법
 * - vTaskDelete()로 태스크 삭제
 * - eTaskGetState()로 태스크 상태 조회
 * 
 * @task_state_diagram
 *                    ┌─────────────────────────────┐
 *                    │                             │
 *                    ▼                             │
 *  ┌─────────┐   Scheduler   ┌─────────┐    vTaskDelay()
 *  │  Ready  │──────────────>│ Running │────────────────┐
 *  └─────────┘               └─────────┘                │
 *       ▲                         │                     │
 *       │                         │                     ▼
 *       │                   vTaskSuspend()        ┌─────────┐
 *       │                         │               │ Blocked │
 *       │                         ▼               └─────────┘
 *       │                   ┌───────────┐              │
 *       └───────────────────│ Suspended │              │
 *          vTaskResume()    └───────────┘              │
 *                                                      │
 *       └──────────────────────────────────────────────┘
 *                      Timeout/Event
 * 
 * @usage
 * Ex02_RunDemo() 함수를 core0_main()에서 호출하세요.
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

#define TASK_PRIORITY       (tskIDLE_PRIORITY + 2)
#define CONTROLLER_PRIORITY (tskIDLE_PRIORITY + 3)

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
static TaskHandle_t xWorkerTaskHandle = NULL;
static TaskHandle_t xControllerTaskHandle = NULL;

/* 태스크 상태 문자열 (디버깅용) */
static const char* const pcStateNames[] = {
    "Running",      /* eRunning = 0 */
    "Ready",        /* eReady = 1 */
    "Blocked",      /* eBlocked = 2 */
    "Suspended",    /* eSuspended = 3 */
    "Deleted"       /* eDeleted = 4 */
};

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvWorkerTask(void *pvParameters);
static void prvControllerTask(void *pvParameters);
static void prvToggleLED(void);
static const char* prvGetTaskStateName(eTaskState eState);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief 태스크 상태를 문자열로 반환
 */
static const char* prvGetTaskStateName(eTaskState eState)
{
    if(eState <= eDeleted)
    {
        return pcStateNames[eState];
    }
    return "Unknown";
}

/**
 * @brief Worker Task - 상태 전이 대상 태스크
 * 
 * 이 태스크는 Controller에 의해 Suspend/Resume 됩니다.
 * LED를 토글하여 현재 Running 상태임을 표시합니다.
 */
static void prvWorkerTask(void *pvParameters)
{
    (void)pvParameters;
    
    const TickType_t xDelay = pdMS_TO_TICKS(200);
    
    for(;;)
    {
        /* LED 토글 - 태스크가 실행 중임을 표시 */
        prvToggleLED();
        
        /* 
         * vTaskDelay() 호출 시:
         * Running → Blocked 상태로 전환
         * 지정된 시간 후 Blocked → Ready 상태로 전환
         */
        vTaskDelay(xDelay);
    }
}

/**
 * @brief Controller Task - Worker 태스크의 상태를 제어
 * 
 * 이 태스크는 Worker 태스크를 주기적으로 Suspend/Resume 합니다.
 * 다양한 상태 전이 시나리오를 실행합니다.
 */
static void prvControllerTask(void *pvParameters)
{
    (void)pvParameters;
    
    eTaskState eCurrentState;
    uint32_t ulCycleCount = 0;
    
    for(;;)
    {
        ulCycleCount++;
        
        /*=====================================================================
         * 시나리오 1: Suspend/Resume 데모
         *=====================================================================*/
        if(xWorkerTaskHandle != NULL)
        {
            /* Worker 태스크의 현재 상태 조회 */
            eCurrentState = eTaskGetState(xWorkerTaskHandle);
            (void)prvGetTaskStateName(eCurrentState);  /* 디버거에서 확인용 */
            
            /* 3초 동안 정상 동작 */
            vTaskDelay(pdMS_TO_TICKS(3000));
            
            /*-----------------------------------------------------------------
             * vTaskSuspend(): 태스크를 Suspended 상태로 전환
             * - Suspended 상태에서는 스케줄러가 태스크를 실행하지 않음
             * - vTaskDelay()와 다르게 자동으로 Ready 상태로 돌아오지 않음
             * - vTaskResume() 호출 시에만 Ready 상태로 전환됨
             *-----------------------------------------------------------------*/
            vTaskSuspend(xWorkerTaskHandle);
            
            /* Worker가 Suspend된 동안 LED는 멈춤 - 2초 대기 */
            vTaskDelay(pdMS_TO_TICKS(2000));
            
            /* Suspend된 상태 확인 */
            eCurrentState = eTaskGetState(xWorkerTaskHandle);
            /* eCurrentState는 eSuspended (3) 이어야 함 */
            
            /*-----------------------------------------------------------------
             * vTaskResume(): Suspended 태스크를 Ready 상태로 전환
             * - 스케줄러가 다시 태스크를 실행 대상으로 고려
             * - 우선순위에 따라 즉시 Running 상태가 될 수 있음
             *-----------------------------------------------------------------*/
            vTaskResume(xWorkerTaskHandle);
            
            /* 다시 LED 깜빡임 시작 - 3초 동안 관찰 */
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
        
        /*=====================================================================
         * 시나리오 2: 자기 자신 Suspend (선택적)
         * 다른 태스크나 ISR에서 Resume 해줘야 함
         *=====================================================================*/
        if(ulCycleCount >= 5)
        {
            /* 5 사이클 후 컨트롤러도 잠시 멈춤 */
            ulCycleCount = 0;
            
            /* 
             * 주의: vTaskSuspend(NULL)은 현재 태스크를 Suspend
             * 이 경우 다른 태스크나 ISR에서 이 태스크를 Resume 해줘야 함
             * 여기서는 예시로 주석 처리
             */
            /* vTaskSuspend(NULL); */
        }
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 02 데모 시작 함수
 */
void Ex02_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /* Worker Task 생성 */
    xTaskCreate(prvWorkerTask,
                "Worker",
                configMINIMAL_STACK_SIZE,
                NULL,
                TASK_PRIORITY,
                &xWorkerTaskHandle);
    
    /* Controller Task 생성 (더 높은 우선순위) */
    xTaskCreate(prvControllerTask,
                "Controller",
                configMINIMAL_STACK_SIZE,
                NULL,
                CONTROLLER_PRIORITY,
                &xControllerTaskHandle);
}

/*********************************************************************************************************************/
/*--------------------------------------------Additional Demo Functions----------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 태스크 삭제 데모
 * 
 * vTaskDelete()를 사용하여 태스크를 삭제하는 방법을 보여줍니다.
 * 주의: 삭제된 태스크는 다시 시작할 수 없습니다.
 * 
 * @param xTaskToDelete 삭제할 태스크의 핸들. NULL이면 호출한 태스크 자신을 삭제.
 */
void vDemoTaskDelete(TaskHandle_t xTaskToDelete)
{
    if(xTaskToDelete != NULL)
    {
        /* 
         * vTaskDelete(): 태스크를 Deleted 상태로 전환하고 리소스 해제
         * - 태스크의 스택 메모리와 TCB는 Idle 태스크에서 해제됨
         * - 한번 삭제된 태스크는 재시작 불가능
         * - 새로운 실행이 필요하면 xTaskCreate()로 다시 생성
         */
        vTaskDelete(xTaskToDelete);
        
        /* 핸들 무효화 */
        if(xTaskToDelete == xWorkerTaskHandle)
        {
            xWorkerTaskHandle = NULL;
        }
    }
}

/**
 * @brief ISR에서 Resume 호출 예시
 * 
 * @note xTaskResumeFromISR()은 ISR 내에서만 호출해야 합니다.
 *       일반 태스크에서는 vTaskResume()을 사용하세요.
 */
void vISR_ResumeWorker(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(xWorkerTaskHandle != NULL)
    {
        /* 
         * xTaskResumeFromISR(): ISR 전용 Resume 함수
         * 반환값이 pdTRUE면 더 높은 우선순위 태스크가 깨어났으므로
         * 컨텍스트 스위칭이 필요함
         */
        xHigherPriorityTaskWoken = xTaskResumeFromISR(xWorkerTaskHandle);
        
        /* 컨텍스트 스위칭 요청 (포트별로 매크로가 다름) */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. 태스크 상태 (Task States)
 *    - Running: 현재 CPU에서 실행 중 (싱글코어에서 항상 1개만 Running)
 *    - Ready: 실행 준비 완료, 스케줄러의 선택 대기
 *    - Blocked: 이벤트/타임아웃 대기 중 (vTaskDelay, Queue, Semaphore 등)
 *    - Suspended: 명시적으로 정지됨, vTaskResume() 필요
 *    - Deleted: 태스크가 삭제되어 Idle 태스크의 메모리 정리 대기
 * 
 * 2. Suspend vs Blocked 차이점
 *    ┌─────────────┬──────────────────────┬──────────────────────┐
 *    │             │ Blocked              │ Suspended            │
 *    ├─────────────┼──────────────────────┼──────────────────────┤
 *    │ 원인        │ vTaskDelay/API 대기  │ vTaskSuspend() 호출  │
 *    │ 복귀        │ 타임아웃/이벤트 자동 │ vTaskResume() 필수   │
 *    │ 대기큐      │ 딜레이/이벤트 큐     │ 아무 큐에도 없음     │
 *    └─────────────┴──────────────────────┴──────────────────────┘
 * 
 * 3. eTaskGetState() 반환값
 *    typedef enum {
 *        eRunning = 0,
 *        eReady,
 *        eBlocked,
 *        eSuspended,
 *        eDeleted
 *    } eTaskState;
 * 
 * 4. 주의사항
 *    - vTaskSuspend(NULL): 자기 자신을 Suspend (누군가 Resume 해줘야 함)
 *    - vTaskDelete(NULL): 자기 자신을 삭제 (반환되지 않음)
 *    - ISR에서는 반드시 FromISR 버전 API 사용
 */
