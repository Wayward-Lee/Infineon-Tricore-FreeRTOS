/**********************************************************************************************************************
 * @file    Ex06_Counting_Semaphore.c
 * @brief   FreeRTOS Example 06: Counting Semaphore Demo (Resource Pool Management)
 * 
 * @description
 * 이 예제는 Counting Semaphore를 사용한 리소스 풀 관리를 학습합니다.
 * 제한된 개수의 리소스(예: 직렬 포트, 메모리 풀)를 여러 태스크가 공유하는 패턴입니다.
 * 
 * @learning_objectives
 * - xSemaphoreCreateCounting()으로 Counting Semaphore 생성
 * - Binary vs Counting Semaphore 차이점
 * - 리소스 풀 관리 패턴 구현
 * - uxSemaphoreGetCount()로 가용 리소스 조회
 * 
 * @resource_pool_pattern
 *     
 *     ┌──────────────────────────────────────────────────────────────┐
 *     │              Counting Semaphore 리소스 풀                    │
 *     ├──────────────────────────────────────────────────────────────┤
 *     │                                                              │
 *     │   초기값 = 3 (3개의 리소스 사용 가능)                        │
 *     │                                                              │
 *     │    Worker1 ──Take──>  [2]  <──Give── Worker1                │
 *     │    Worker2 ──Take──>  [1]  <──Give── Worker2                │
 *     │    Worker3 ──Take──>  [0]  <──Give── Worker3                │
 *     │    Worker4 ──Take──>  (블로킹 대기)                         │
 *     │                                                              │
 *     │   값 = 현재 사용 가능한 리소스 개수                         │
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
#include "semphr.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* 리소스 풀 설정 */
#define MAX_RESOURCES       3       /* 총 리소스 개수 */
#define NUM_WORKERS         5       /* Worker 태스크 개수 (리소스보다 많음) */

/* 작업 시간 (밀리초) */
#define WORK_TIME_MS        500     /* 리소스 사용 시간 */
#define WAIT_TIMEOUT_MS     2000    /* 리소스 대기 최대 시간 */

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* Counting Semaphore 핸들 */
static SemaphoreHandle_t xResourceSemaphore = NULL;

/* 태스크 핸들 */
static TaskHandle_t xWorkerTaskHandles[NUM_WORKERS] = {NULL};

/* 통계 */
static volatile uint32_t g_ulResourceAcquired[NUM_WORKERS] = {0};   /* 각 Worker별 리소스 획득 횟수 */
static volatile uint32_t g_ulResourceTimeout[NUM_WORKERS] = {0};    /* 각 Worker별 타임아웃 횟수 */

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvWorkerTask(void *pvParameters);
static void prvUseResource(uint32_t ulWorkerId);
static void prvToggleLED(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief 리소스 사용 시뮬레이션
 * 
 * 실제 프로젝트에서는 다음과 같은 리소스 사용이 될 수 있습니다:
 * - 직렬 포트 통신
 * - 메모리 버퍼 사용
 * - 외부 장치 접근
 */
static void prvUseResource(uint32_t ulWorkerId)
{
    /* 리소스 사용 중... */
    (void)ulWorkerId;
    
    /* LED 토글로 리소스 사용 표시 */
    prvToggleLED();
    
    /* 작업 시간 시뮬레이션 */
    vTaskDelay(pdMS_TO_TICKS(WORK_TIME_MS));
}

/**
 * @brief Worker Task - 리소스를 요청하고 사용
 * 
 * 여러 Worker 태스크가 제한된 리소스를 공유합니다.
 * Counting Semaphore로 동시 접근을 제어합니다.
 */
static void prvWorkerTask(void *pvParameters)
{
    uint32_t ulWorkerId = (uint32_t)(uintptr_t)pvParameters;
    UBaseType_t uxAvailableResources;
    BaseType_t xSemaphoreResult;
    
    for(;;)
    {
        /* 현재 가용 리소스 확인 */
        uxAvailableResources = uxSemaphoreGetCount(xResourceSemaphore);
        (void)uxAvailableResources;  /* 디버거에서 확인용 */
        
        /*---------------------------------------------------------------------
         * xSemaphoreTake(): 리소스 요청 (카운트 감소)
         * 
         * Counting Semaphore에서:
         * - 값 > 0: 값을 1 감소시키고 즉시 반환
         * - 값 == 0: 지정된 시간만큼 블로킹
         * 
         * 타임아웃 설정:
         * - portMAX_DELAY: 무한 대기
         * - pdMS_TO_TICKS(2000): 2초 대기 후 타임아웃
         *---------------------------------------------------------------------*/
        xSemaphoreResult = xSemaphoreTake(xResourceSemaphore, 
                                          pdMS_TO_TICKS(WAIT_TIMEOUT_MS));
        
        if(xSemaphoreResult == pdPASS)
        {
            /* 리소스 획득 성공 */
            g_ulResourceAcquired[ulWorkerId]++;
            
            /*-----------------------------------------------------------------
             * 리소스 사용 영역 (Critical Section과 다름!)
             * 
             * 주의: 이 영역에서는 리소스를 사용하지만,
             *       다른 태스크는 "남은" 리소스를 사용 가능
             *-----------------------------------------------------------------*/
            prvUseResource(ulWorkerId);
            
            /*-----------------------------------------------------------------
             * xSemaphoreGive(): 리소스 반환 (카운트 증가)
             * 
             * 반드시 Take 후에 Give 해야 함!
             * Give 하지 않으면 리소스 누수 발생
             *-----------------------------------------------------------------*/
            xSemaphoreGive(xResourceSemaphore);
        }
        else
        {
            /* 타임아웃: 리소스 획득 실패 */
            g_ulResourceTimeout[ulWorkerId]++;
            
            /* 
             * 타임아웃 시 처리 예:
             * - 에러 로깅
             * - 대체 동작 수행
             * - 나중에 재시도
             */
        }
        
        /* 다음 작업까지 대기 */
        vTaskDelay(pdMS_TO_TICKS(100));  /* 작업 주기 */
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 06 데모 시작 함수
 */
void Ex06_RunDemo(void)
{
    uint32_t i;
    char pcTaskName[16];
    
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /*-------------------------------------------------------------------------
     * xSemaphoreCreateCounting(): Counting Semaphore 생성
     * 
     * 파라미터:
     * - uxMaxCount: 세마포어 최대 값 (리소스 최대 개수)
     * - uxInitialCount: 초기 값 (처음 사용 가능한 리소스 개수)
     * 
     * 리소스 풀 패턴:
     *   xSemaphoreCreateCounting(MAX, MAX)  ← 모든 리소스 사용 가능
     * 
     * 이벤트 카운팅 패턴:
     *   xSemaphoreCreateCounting(MAX, 0)    ← 이벤트 발생 시 증가
     *-------------------------------------------------------------------------*/
    xResourceSemaphore = xSemaphoreCreateCounting(MAX_RESOURCES, MAX_RESOURCES);
    
    if(xResourceSemaphore != NULL)
    {
        /* Worker 태스크들 생성 */
        for(i = 0; i < NUM_WORKERS; i++)
        {
            /* 태스크 이름 생성: "Worker0", "Worker1", ... */
            pcTaskName[0] = 'W';
            pcTaskName[1] = 'o';
            pcTaskName[2] = 'r';
            pcTaskName[3] = 'k';
            pcTaskName[4] = 'e';
            pcTaskName[5] = 'r';
            pcTaskName[6] = '0' + (char)i;
            pcTaskName[7] = '\0';
            
            xTaskCreate(prvWorkerTask,
                        pcTaskName,
                        configMINIMAL_STACK_SIZE,
                        (void*)(uintptr_t)i,    /* Worker ID 전달 */
                        tskIDLE_PRIORITY + 1,
                        &xWorkerTaskHandles[i]);
        }
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------Event Counting Example-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 이벤트 카운팅 패턴 예시
 * 
 * Binary Semaphore는 연속 이벤트 발생 시 하나만 기록되지만,
 * Counting Semaphore는 모든 이벤트를 카운팅합니다.
 */
void vEventCountingDemo(void)
{
    SemaphoreHandle_t xEventSemaphore;
    
    /* 이벤트 카운팅용: 최대 10개, 초기값 0 */
    xEventSemaphore = xSemaphoreCreateCounting(10, 0);
    
    if(xEventSemaphore != NULL)
    {
        /* 연속 이벤트 발생 시뮬레이션 */
        xSemaphoreGive(xEventSemaphore);  /* count = 1 */
        xSemaphoreGive(xEventSemaphore);  /* count = 2 */
        xSemaphoreGive(xEventSemaphore);  /* count = 3 */
        
        /* 모든 이벤트 처리 */
        while(xSemaphoreTake(xEventSemaphore, 0) == pdPASS)
        {
            /* 각 이벤트 처리... */
        }
        
        /* 정리 */
        vSemaphoreDelete(xEventSemaphore);
    }
}

/**
 * @brief 통계 조회
 */
void vGetCountingSemaphoreStats(uint32_t ulWorkerId, 
                                uint32_t *pulAcquired, 
                                uint32_t *pulTimeout)
{
    if(ulWorkerId < NUM_WORKERS)
    {
        if(pulAcquired != NULL) *pulAcquired = g_ulResourceAcquired[ulWorkerId];
        if(pulTimeout != NULL) *pulTimeout = g_ulResourceTimeout[ulWorkerId];
    }
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. Binary vs Counting Semaphore
 *    ┌─────────────────────┬────────────────┬────────────────────┐
 *    │ 특성                │ Binary         │ Counting           │
 *    ├─────────────────────┼────────────────┼────────────────────┤
 *    │ 최대 값             │ 1              │ N (설정 가능)      │
 *    │ 연속 Give 시        │ 1개만 기록     │ 모두 카운트        │
 *    │ 주요 용도           │ 이벤트 시그널  │ 리소스 풀/카운팅   │
 *    └─────────────────────┴────────────────┴────────────────────┘
 * 
 * 2. Counting Semaphore 생성
 *    // 리소스 풀: 초기값 = 최대값
 *    xSemaphoreCreateCounting(5, 5);  // 5개 리소스 모두 사용 가능
 *    
 *    // 이벤트 카운트: 초기값 = 0
 *    xSemaphoreCreateCounting(10, 0);  // 이벤트 발생 시 증가
 * 
 * 3. 리소스 풀 패턴
 *    
 *    초기: [3] → 3개 리소스 사용 가능
 *    
 *    Worker1 Take: [3] → [2]  ← 리소스 획득
 *    Worker2 Take: [2] → [1]
 *    Worker3 Take: [1] → [0]
 *    Worker4 Take: [0] → (블로킹) ← 리소스 없음
 *    
 *    Worker1 Give: [0] → [1]  ← 리소스 반환
 *    Worker4 깨어남: [1] → [0]  ← 대기하던 태스크가 리소스 획득
 * 
 * 4. 이 예제의 구성
 *    - 리소스: 3개
 *    - Worker: 5개 (리소스보다 많음)
 *    - 결과: 동시에 최대 3개 Worker만 리소스 사용 가능
 *            나머지 2개는 대기하거나 타임아웃
 * 
 * 5. 실제 사용 예
 *    - 직렬 포트 풀 (예: 3개의 UART 중 하나 사용)
 *    - 메모리 버퍼 풀 (예: DMA 버퍼 5개 중 하나 할당)
 *    - 커넥션 풀 (예: 데이터베이스 연결 제한)
 *    - 프린터 큐 (예: 동시 인쇄 작업 제한)
 * 
 * 6. 주의사항
 *    - Take 후 반드시 Give 호출 (리소스 누수 방지)
 *    - Give를 Take보다 많이 하면 안됨 (최대값 초과 불가)
 *    - 오류 처리 시에도 Give 보장 필요 (finally 패턴)
 */
