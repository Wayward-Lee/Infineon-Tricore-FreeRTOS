/**********************************************************************************************************************
 * @file    Ex13_UART_Queue.c
 * @brief   FreeRTOS Example 13: UART Communication with Queue (Practical Demo)
 * 
 * @description
 * 이 예제는 UART 통신에서 Queue를 이용한 데이터 버퍼링을 학습합니다.
 * 실제 임베디드 시스템에서 널리 사용되는 명령 파싱 패턴을 구현합니다.
 * 
 * @note
 * 이 예제는 UART 하드웨어 설정이 필요합니다.
 * DAVE에서 UART APP을 추가하거나 iLLD의 UART 드라이버를 사용하세요.
 * 
 * @learning_objectives
 * - UART RX 데이터의 Queue 버퍼링
 * - ISR에서 Queue로 데이터 전송
 * - 명령 파서 태스크 구현
 * - 텍스트 기반 명령 처리
 * 
 * @architecture
 *     
 *     ┌──────────────────────────────────────────────────────────────┐
 *     │               UART + Queue 아키텍처                         │
 *     ├──────────────────────────────────────────────────────────────┤
 *     │                                                              │
 *     │  ┌─────────┐     ┌──────────┐     ┌──────────────────┐      │
 *     │  │  UART   │────>│ RX Queue │────>│  Command Parser  │      │
 *     │  │  ISR    │ Char│ (버퍼)   │     │     Task         │      │
 *     │  └─────────┘     └──────────┘     └────────┬─────────┘      │
 *     │                                            │                │
 *     │                                            ▼                │
 *     │                                   ┌──────────────────┐      │
 *     │                                   │  Command Handler │      │
 *     │                                   │  (LED/상태 제어) │      │
 *     │                                   └──────────────────┘      │
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
#include "semphr.h"

#include <string.h>

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define LED_PORT            &MODULE_P22
#define LED_PIN             5

/* 큐 설정 */
#define RX_QUEUE_LENGTH     64      /* RX 버퍼 크기 */
#define CMD_BUFFER_SIZE     32      /* 명령 버퍼 크기 */

/*********************************************************************************************************************/
/*----------------------------------------------------Data Types-----------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 명령 종류
 */
typedef enum
{
    CMD_NONE = 0,
    CMD_LED_ON,             /* "LED ON" */
    CMD_LED_OFF,            /* "LED OFF" */
    CMD_LED_TOGGLE,         /* "LED TOGGLE" */
    CMD_STATUS,             /* "STATUS" */
    CMD_HELP,               /* "HELP" */
    CMD_UNKNOWN             /* 알 수 없는 명령 */
} CommandType_t;

/*********************************************************************************************************************/
/*-------------------------------------------------Global Variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* 큐 핸들 */
static QueueHandle_t xRxQueue = NULL;

/* 태스크 핸들 */
static TaskHandle_t xParserTaskHandle = NULL;

/* LED 상태 */
static volatile BaseType_t g_xLEDState = pdFALSE;

/* 통계 */
static volatile uint32_t g_ulRxCount = 0;
static volatile uint32_t g_ulCmdCount = 0;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void prvCommandParserTask(void *pvParameters);
static CommandType_t prvParseCommand(const char *pcCommand);
static void prvExecuteCommand(CommandType_t eCmd);
static void prvSetLED(BaseType_t xState);
static void prvSimulateUartRx(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void prvSetLED(BaseType_t xState)
{
    g_xLEDState = xState;
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
 * @brief 명령 파싱
 * 
 * 텍스트 명령을 분석하여 명령 종류를 반환합니다.
 */
static CommandType_t prvParseCommand(const char *pcCommand)
{
    /* 명령 비교 (대소문자 무시 버전도 구현 가능) */
    if(strcmp(pcCommand, "LED ON") == 0)
    {
        return CMD_LED_ON;
    }
    else if(strcmp(pcCommand, "LED OFF") == 0)
    {
        return CMD_LED_OFF;
    }
    else if(strcmp(pcCommand, "LED TOGGLE") == 0)
    {
        return CMD_LED_TOGGLE;
    }
    else if(strcmp(pcCommand, "STATUS") == 0)
    {
        return CMD_STATUS;
    }
    else if(strcmp(pcCommand, "HELP") == 0)
    {
        return CMD_HELP;
    }
    else if(strlen(pcCommand) > 0)
    {
        return CMD_UNKNOWN;
    }
    
    return CMD_NONE;
}

/**
 * @brief 명령 실행
 */
static void prvExecuteCommand(CommandType_t eCmd)
{
    switch(eCmd)
    {
        case CMD_LED_ON:
            prvSetLED(pdTRUE);
            /* UART TX: "LED is ON\r\n" */
            break;
            
        case CMD_LED_OFF:
            prvSetLED(pdFALSE);
            /* UART TX: "LED is OFF\r\n" */
            break;
            
        case CMD_LED_TOGGLE:
            prvSetLED(!g_xLEDState);
            /* UART TX: "LED toggled\r\n" */
            break;
            
        case CMD_STATUS:
            /* UART TX: 현재 상태 정보 출력 */
            break;
            
        case CMD_HELP:
            /* UART TX: 도움말 출력 */
            /* "Commands: LED ON, LED OFF, LED TOGGLE, STATUS, HELP\r\n" */
            break;
            
        case CMD_UNKNOWN:
            /* UART TX: "Unknown command\r\n" */
            break;
            
        default:
            break;
    }
    
    if(eCmd != CMD_NONE)
    {
        g_ulCmdCount++;
    }
}

/**
 * @brief Command Parser Task
 * 
 * RX Queue에서 문자를 읽어 명령을 파싱하고 실행합니다.
 */
static void prvCommandParserTask(void *pvParameters)
{
    (void)pvParameters;
    
    char cReceivedChar;
    char acCmdBuffer[CMD_BUFFER_SIZE];
    uint32_t ulCmdIndex = 0;
    CommandType_t eCommand;
    
    /* 버퍼 초기화 */
    memset(acCmdBuffer, 0, sizeof(acCmdBuffer));
    
    for(;;)
    {
        /*---------------------------------------------------------------------
         * xQueueReceive(): Queue에서 문자 수신
         * 
         * UART ISR에서 전송된 문자를 Queue에서 꺼내옵니다.
         * Queue가 비어있으면 블로킹됩니다.
         *---------------------------------------------------------------------*/
        if(xQueueReceive(xRxQueue, &cReceivedChar, portMAX_DELAY) == pdPASS)
        {
            g_ulRxCount++;
            
            /* 줄 끝 문자 처리 (CR 또는 LF) */
            if(cReceivedChar == '\r' || cReceivedChar == '\n')
            {
                if(ulCmdIndex > 0)
                {
                    /* 명령 문자열 종료 */
                    acCmdBuffer[ulCmdIndex] = '\0';
                    
                    /* 명령 파싱 및 실행 */
                    eCommand = prvParseCommand(acCmdBuffer);
                    prvExecuteCommand(eCommand);
                    
                    /* 버퍼 초기화 */
                    memset(acCmdBuffer, 0, sizeof(acCmdBuffer));
                    ulCmdIndex = 0;
                }
            }
            else if(cReceivedChar == '\b' || cReceivedChar == 0x7F)  /* Backspace */
            {
                if(ulCmdIndex > 0)
                {
                    ulCmdIndex--;
                    acCmdBuffer[ulCmdIndex] = '\0';
                }
            }
            else
            {
                /* 일반 문자 추가 */
                if(ulCmdIndex < (CMD_BUFFER_SIZE - 1))
                {
                    acCmdBuffer[ulCmdIndex] = cReceivedChar;
                    ulCmdIndex++;
                }
                /* 버퍼 오버플로우 시 무시 */
            }
        }
    }
}

/**
 * @brief UART RX 시뮬레이션
 * 
 * 실제 시스템에서는 이 함수가 UART RX ISR로 대체됩니다.
 */
static void prvSimulateUartRx(void)
{
    /* 시뮬레이션용 명령 */
    static const char *apcCommands[] = {
        "LED ON\r",
        "LED TOGGLE\r",
        "STATUS\r",
        "LED OFF\r",
        "HELP\r"
    };
    static uint32_t ulCmdIndex = 0;
    static uint32_t ulCharIndex = 0;
    static TickType_t xLastTime = 0;
    
    const char *pcCurrentCmd = apcCommands[ulCmdIndex];
    char cChar;
    
    /* 50ms마다 한 문자씩 전송 시뮬레이션 */
    if((xTaskGetTickCount() - xLastTime) >= pdMS_TO_TICKS(50))
    {
        xLastTime = xTaskGetTickCount();
        
        cChar = pcCurrentCmd[ulCharIndex];
        
        if(cChar != '\0')
        {
            /* RX Queue로 문자 전송 (ISR처럼) */
            xQueueSend(xRxQueue, &cChar, 0);
            ulCharIndex++;
        }
        else
        {
            /* 다음 명령 */
            ulCmdIndex = (ulCmdIndex + 1) % (sizeof(apcCommands) / sizeof(apcCommands[0]));
            ulCharIndex = 0;
        }
    }
}

/*********************************************************************************************************************/
/*---------------------------------------------UART ISR Template-----------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief UART RX 인터럽트 핸들러 템플릿
 * 
 * 실제 시스템에서 UART 인터럽트에 연결하세요.
 * 
 * @param ucReceivedByte UART에서 수신된 바이트
 */
void vUART_RxISR(uint8_t ucReceivedByte)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    char cChar = (char)ucReceivedByte;
    
    if(xRxQueue != NULL)
    {
        /*-----------------------------------------------------------------
         * xQueueSendFromISR(): ISR에서 Queue로 데이터 전송
         * 
         * 주의사항:
         * - ISR에서는 반드시 FromISR 버전 사용
         * - 타임아웃 불가 (즉시 반환)
         * - xHigherPriorityTaskWoken 처리 필수
         *-----------------------------------------------------------------*/
        xQueueSendFromISR(xRxQueue, &cChar, &xHigherPriorityTaskWoken);
        
        /* 더 높은 우선순위 태스크가 깨어났으면 컨텍스트 스위칭 */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/*********************************************************************************************************************/
/*--------------------------------------------------Main Entry Point-------------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief 예제 13 데모 시작 함수
 */
void Ex13_RunDemo(void)
{
    /* LED 핀 초기화 */
    IfxPort_setPinModeOutput(LED_PORT, LED_PIN, 
                             IfxPort_OutputMode_pushPull, 
                             IfxPort_OutputIdx_general);
    prvSetLED(pdFALSE);
    
    /* 
     * UART 초기화 - 실제 시스템에서 필요
     * DAVE UART APP 또는 iLLD 드라이버 사용
     */
    /* UART_Init(&UART_0); */
    
    /* RX Queue 생성 (문자 단위 버퍼) */
    xRxQueue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(char));
    
    if(xRxQueue != NULL)
    {
        /* Command Parser Task 생성 */
        xTaskCreate(prvCommandParserTask,
                    "CmdParser",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    tskIDLE_PRIORITY + 2,
                    &xParserTaskHandle);
    }
    
    /* 
     * 참고: 실제 시스템에서는 UART 인터럽트가 xRxQueue에 문자를 전송합니다.
     * 이 예제에서는 prvSimulateUartRx()로 시뮬레이션합니다.
     */
}

/*********************************************************************************************************************/
/*-----------------------------------------Simulation Task (Demo Only)-----------------------------------------------*/
/*********************************************************************************************************************/

/**
 * @brief UART RX 시뮬레이션 태스크 (데모용)
 * 
 * 실제 시스템에서는 이 태스크가 필요 없습니다.
 */
void vUartSimulationTask(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        prvSimulateUartRx();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief 통계 조회
 */
void vGetUartStats(uint32_t *pulRxCount, uint32_t *pulCmdCount)
{
    if(pulRxCount != NULL) *pulRxCount = g_ulRxCount;
    if(pulCmdCount != NULL) *pulCmdCount = g_ulCmdCount;
}

/*********************************************************************************************************************/
/*-------------------------------------------Learning Points Summary-------------------------------------------------*/
/*********************************************************************************************************************/
/*
 * 📚 학습 포인트 정리:
 * 
 * 1. ISR-Task 분리 패턴
 *    
 *    ┌─────┐   1 문자   ┌───────┐   1 명령   ┌─────────┐
 *    │ ISR │──────────>│ Queue │──────────>│  Task   │
 *    │(빠름)│           │(버퍼) │           │(느려도됨)│
 *    └─────┘           └───────┘           └─────────┘
 *    
 *    장점:
 *    - ISR 실행 시간 최소화
 *    - 데이터 손실 방지 (버퍼링)
 *    - 복잡한 파싱은 태스크에서
 * 
 * 2. 문자 수준 vs 라인 수준 Queue
 *    
 *    문자 수준 (이 예제):
 *      - Queue에 한 문자씩 저장
 *      - 유연한 명령 파싱
 *      - 메모리 효율적
 *    
 *    라인 수준 (대안):
 *      - ISR에서 '\n'까지 버퍼링 후 Queue에 라인 전체 전송
 *      - 더 큰 버퍼 필요
 *      - 파싱 태스크 단순화
 * 
 * 3. 명령 파서 구조
 *    
 *    입력: "LED ON\r"
 *          ↓
 *    [문자 수집] → ['\r' 감지] → [파싱] → [실행]
 *          ↓
 *    버퍼: "LED ON"
 * 
 * 4. FromISR API 규칙
 *    
 *    ✅ xQueueSendFromISR()
 *    ✅ xSemaphoreGiveFromISR()
 *    ✅ xTaskNotifyFromISR()
 *    
 *    ❌ xQueueSend() - ISR에서 사용 금지!
 *    ❌ xSemaphoreTake() - ISR에서 블로킹 불가!
 * 
 * 5. 실제 구현 시 고려사항
 *    - 버퍼 오버플로우 처리
 *    - 명령 에코 (선택적)
 *    - 타임아웃 (불완전 명령 처리)
 *    - 프레이밍 (시작/끝 마커)
 *    - 체크섬/CRC (바이너리 프로토콜)
 */
