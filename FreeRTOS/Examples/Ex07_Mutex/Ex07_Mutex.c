/**********************************************************************************************************************
 * @file    Ex07_Mutex.c
 * @brief   FreeRTOS Example 07: Mutex Demo (Resource Protection with Priority Inheritance)
 * 
 * @description
 * 이 예제는 Mutex를 사용한 공유 자원 보호를 학습합니다.
 * Binary Semaphore와의 차이점과 Priority Inheritance 동작을 실습합니다.
 * 
 * @learning_objectives
 * - xSemaphoreCreateMutex()로 Mutex 생성
 * - Mutex와 Binary Semaphore의 차이점 이해
 * - Priority Inheritance 메커니즘 이해
 * - 공유 자원 보호 패턴 구현
 * 
 * @priority_inheritance
 *     
 *     ┌──────────────────────────────────────────────────────────────────┐
 *     │                 Priority Inheritance 문제와 해결                 │
 *     ├──────────────────────────────────────────────────────────────────┤
 *     │                                                                  │
 *     │  문제 상황 (Priority Inversion):                                │
 *     │    1. Low Priority 태스크가 Mutex 획득                          │
 *     │    2. High Priority 태스크가 Mutex 대기 (블로킹)                │
 *     │    3. Medium Priority 태스크 실행 → Low 선점                   │
 *     │    4. High가 Medium보다 오래 대기! (우선순위 역전)              │
 *     │                                                                  │
 *     │  해결 (Priority Inheritance):                                   │
 *     │    2. High가 대기할 때 Low의 우선순위를 High로 일시 상승        │
 *     │    3. Low가 Mutex 해제하면 원래 우선순위로 복원                 │
 *     │    → Medium이 Low를 선점하지 못함                              │
 *     │                                                                  │
 *     └──────────────────────────────────────────────────────────────────┘
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

/* 태스크 우선순위 */
#define PRIORITY_HIGH       (tskIDLE_PRIORITY + 3)
#define PRIORITY_MEDIUM     (tskIDLE_PRIORITY + 2)
#define PRIORITY_LOW        (tskIDLE_PRIORITY + 1)

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* Mutex 핸들 */
static SemaphoreHandle_t xMutex = NULL;

/* 태스크 핸들 */
static TaskHandle_t xLowPriorityTaskHandle = NULL;
static TaskHandle_t xMediumPriorityTaskHandle = NULL;
static TaskHandle_t xHighPriorityTaskHandle = NULL;

/* 공유 자원 (보호 대상) */
static volatile uint32_t g_ulSharedCounter = 0;

/* Critical Section 진입 횟수 */
static volatile uint32_t g_ulLowCSCount = 0;
static volatile uint32_t g_ulHighCSCount = 0;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvLowPriorityTask(void *pvParameters);
static void prvMediumPriorityTask(void *pvParameters);
static void prvHighPriorityTask(void *pvParameters);
static void prvAccessSharedResource(const char* pcTaskName);
static void prvToggleLED(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvToggleLED(void)
{
    IfxPort_togglePin(LED_PORT, LED_PIN);
}

/**
 * @brief 공유 자원 접근 (Mutex 보호 필요)
 * 
 * 이 함수는 공유 카운터를 증가시킵니다.
 * Mutex 없이 호출하면 데이터 경쟁(Race Condition) 발생 가능.
 */
static void prvAccessSharedResource(const char* pcTaskName)
{
    uint32_t ulLocalCopy;
    
    (void)pcTaskName;  /* 디버깅용 */
    
    /* 
     * 비원자적 연산 시뮬레이션:
     * 읽기 → 수정 → 쓰기 사이에 다른 태스크가 개입하면
     * 데이터 손상 발생
     */
    ulLocalCopy = g_ulSharedCounter;
    
    /* 시간 지연 (다른 태스크 개입 가능성 증가) */
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ulLocalCopy++;
    g_ulSharedCounter = ulLocalCopy;
    
    /* LED 토글로 자원 접근 표시 */
    prvToggleLED();
}

/**
 * @brief Low Priority Task - Mutex 홀더
 * 
 * 낮은 우선순위로 Mutex를 획득하고 오래 보유합니다.
 * Priority Inheritance가 동작하면 High Priority Task가 대기할 때
 * 이 태스크의 우선순위가 일시적으로 상승합니다.
 */
static void prvLowPriorityTask(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        /*---------------------------------------------------------------------
         * Mutex 획득 시도
         * 
         * xSemaphoreTake()는 Mutex와 Semaphore 모두에서 사용
         * Mutex 특징:
         * - 획득한 태스크만 해제 가능 (소유권 개념)
         * - Priority Inheritance 지원
         * - 재귀 호출 시 Recursive Mutex 필요
         *---------------------------------------------------------------------*/
        if(xSemaphoreTake(xMutex, portMAX_DELAY) == pdPASS)
        {
            /* Critical Section 시작 */
            g_ulLowCSCount++;
            
            /*-----------------------------------------------------------------
             * 긴 시간 동안 Mutex 보유
             * 
             * 이 동안:
             * 1. High Priority Task가 Mutex 대기 시작
             * 2. Priority Inheritance 발생:
             *    → Low의 우선순위가 High로 상승
             * 3. Medium Priority Task가 실행되지 못함
             *    (Low가 이제 High 우선순위이므로)
             *-----------------------------------------------------------------*/
            prvAccessSharedResource("Low");
            
            /* 추가 작업 시뮬레이션 */
            vTaskDelay(pdMS_TO_TICKS(100));
            
            /* Mutex 해제 */
            xSemaphoreGive(xMutex);
            /* Critical Section 종료 → 우선순위 원래대로 복원 */
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief Medium Priority Task - 간섭자 역할
 * 
 * Priority Inversion 문제를 보여주기 위한 태스크입니다.
 * Priority Inheritance가 없으면 이 태스크가 Low를 선점하여
 * High가 오래 대기하게 됩니다.
 */
static void prvMediumPriorityTask(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        /* 
         * Mutex를 사용하지 않는 작업
         * Priority Inheritance가 동작하면:
         *   → Low가 High 우선순위로 상승되어 이 태스크보다 먼저 실행
         * 
         * Priority Inheritance가 없으면:
         *   → Medium이 Low를 선점하여 High가 오래 대기
         */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @brief High Priority Task - Mutex 대기자
 * 
 * 높은 우선순위로 Mutex를 요청합니다.
 * Low가 Mutex를 보유 중이면 Priority Inheritance가 발생합니다.
 */
static void prvHighPriorityTask(void *pvParameters)
{
    (void)pvParameters;
    
    /* Low Priority Task가 먼저 Mutex 획득할 시간 제공 */
    vTaskDelay(pdMS_TO_TICKS(50));
    
    for(;;)
    {
        /* Mutex 획득 시도 */
        if(xSemaphoreTake(xMutex, portMAX_DELAY) == pdPASS)
        {
            /* Critical Section */
            g_ulHighCSCount++;
            
            prvAccessSharedResource("High");
            
            /* Mutex 해제 */
            xSemaphoreGive(xMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 07 데모 시작 함수
 */
void Ex07_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    
    /*-------------------------------------------------------------------------
     * xSemaphoreCreateMutex(): Mutex 생성
     * 
     * 특징:
     * - 초기 상태: 사용 가능 (Binary Sema와 다름!)
     * - Priority Inheritance 지원
     * - 소유권 개념: 획득한 태스크만 해제 가능
     * 
     * vs Binary Semaphore:
     * - Mutex: 자원 보호 (상호 배제) 용도
     * - Binary Sema: 동기화/시그널링 용도
     *-------------------------------------------------------------------------*/
    xMutex = xSemaphoreCreateMutex();
    
    if(xMutex != NULL)
    {
        /* Low Priority Task 생성 */
        xTaskCreate(prvLowPriorityTask,
                    "Low",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    PRIORITY_LOW,
                    &xLowPriorityTaskHandle);
        
        /* Medium Priority Task 생성 */
        xTaskCreate(prvMediumPriorityTask,
                    "Medium",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    PRIORITY_MEDIUM,
                    &xMediumPriorityTaskHandle);
        
        /* High Priority Task 생성 */
        xTaskCreate(prvHighPriorityTask,
                    "High",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    PRIORITY_HIGH,
                    &xHighPriorityTaskHandle);
    }
}

/*********************************************************************************************************************/
/*-----------------------------------------Recursive Mutex Example---------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief Recursive Mutex 예시
 * 
 * 같은 태스크가 Mutex를 여러 번 획득해야 할 때 사용합니다.
 * 예: 재귀 함수에서 자원 보호
 */
void vRecursiveMutexDemo(void)
{
    SemaphoreHandle_t xRecursiveMutex;
    
    /*-------------------------------------------------------------------------
     * xSemaphoreCreateRecursiveMutex(): Recursive Mutex 생성
     * 
     * 특징:
     * - 같은 태스크가 여러 번 Take 가능
     * - Take 횟수만큼 Give 해야 완전히 해제
     *-------------------------------------------------------------------------*/
    xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
    
    if(xRecursiveMutex != NULL)
    {
        /* 첫 번째 획득 */
        xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
        
        /* 두 번째 획득 (일반 Mutex면 데드락!) */
        xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
        
        /* 작업 수행... */
        
        /* 두 번 Give 필요 */
        xSemaphoreGiveRecursive(xRecursiveMutex);
        xSemaphoreGiveRecursive(xRecursiveMutex);
        
        /* 정리 */
        vSemaphoreDelete(xRecursiveMutex);
    }
}

/**
 * @brief 통계 조회
 */
void vGetMutexStats(uint32_t *pulLowCS, uint32_t *pulHighCS, uint32_t *pulSharedCounter)
{
    if(pulLowCS != NULL) *pulLowCS = g_ulLowCSCount;
    if(pulHighCS != NULL) *pulHighCS = g_ulHighCSCount;
    if(pulSharedCounter != NULL) *pulSharedCounter = g_ulSharedCounter;
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. Mutex vs Binary Semaphore
 *    ┌─────────────────────┬─────────────────┬────────────────────┐
 *    │ 특성                │ Mutex           │ Binary Semaphore   │
 *    ├─────────────────────┼─────────────────┼────────────────────┤
 *    │ 주요 용도           │ 상호 배제       │ 동기화/시그널링    │
 *    │ 초기 상태           │ 사용 가능       │ Empty (Take 블로킹)│
 *    │ 소유권              │ 있음            │ 없음               │
 *    │ Give 태스크         │ Take한 태스크만 │ 아무나 가능        │
 *    │ Priority Inheritance│ 있음            │ 없음               │
 *    │ 재귀 획득           │ Recursive만     │ 불가능             │
 *    └─────────────────────┴─────────────────┴────────────────────┘
 * 
 * 2. Priority Inversion 문제
 *    
 *    시간 →                        
 *    High   ████████░░░░░░░░░████  (Mutex 대기로 블로킹)
 *    Medium ░░░░░░░░████████░░░░░  (Low 선점!)
 *    Low    █░░░░░░░░░░░░░░░█░░░░  (Mutex 보유)
 *    
 *    문제: High가 Medium보다 오래 대기!
 * 
 * 3. Priority Inheritance 해결
 *    
 *    시간 →
 *    High   ████████░░░░░░░░░████
 *    Medium ░░░░░░░░░░░░████░░░░░  (Low가 High 우선순위이므로 대기)
 *    Low    █████████████░░░█░░░░  (High 대기 시 우선순위 상승)
 *    
 *    해결: Low가 High 우선순위로 실행되어 빨리 Mutex 해제
 * 
 * 4. Mutex 사용 패턴
 *    
 *    // 기본 패턴
 *    if(xSemaphoreTake(xMutex, timeout) == pdPASS) {
 *        // Critical Section: 공유 자원 접근
 *        xSemaphoreGive(xMutex);  // 반드시 해제!
 *    }
 *    
 *    // 재귀 패턴
 *    void recursiveFunc() {
 *        xSemaphoreTakeRecursive(xRecursiveMutex, ...);
 *        recursiveFunc();  // 같은 태스크에서 다시 획득
 *        xSemaphoreGiveRecursive(xRecursiveMutex);
 *    }
 * 
 * 5. 주의사항
 *    - Mutex는 태스크 컨텍스트에서만 사용 (ISR에서 사용 금지!)
 *    - 데드락 주의: 여러 Mutex 획득 순서 일관성 유지
 *    - Take 했으면 반드시 Give (예외 상황에서도)
 *    - 가능한 짧은 시간만 Mutex 보유
 */
