/* Standard includes. */
#include <stdio.h>
#include <conio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/* Hardware simulator utility functions */
#include "HW_access.h"

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH (0)

/* TASK PRIORITIES */
#define	TASK_SERIAL_SEND_PRI		(2 + tskIDLE_PRIORITY  )
#define TASK_SERIAL_REC_PRI			(3+ tskIDLE_PRIORITY )
#define	SERVICE_TASK_PRI		(1+ tskIDLE_PRIORITY )

#define portINTERRUPT_SRL_OIC 5

/* TASKS: FORWARD DECLARATIONS */
void prvSerialReceiveTask_0(void* pvParameters);
void prvSerialReceiveTask_1(void* pvParameters);


SemaphoreHandle_t RXC_BS_0, RXC_BS_1;
SemaphoreHandle_t LED_INT_BinarySemaphore;

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH_0 (0)
#define COM_CH_1 (1)

/* TASKS: FORWARD DECLARATIONS */
void led_bar_tsk(void* pvParameters);
void SerialSend_Task(void* pvParameters);

void vApplicationIdleHook(void);

/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
const char trigger[] = "Pozdrav svima\n";
unsigned volatile t_point;
int per_TimerHandle;

/* RECEPTION DATA BUFFER */
#define R_BUF_SIZE (32)
uint8_t r_buffer[R_BUF_SIZE];
unsigned volatile r_point;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const unsigned char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

/* GLOBAL OS-HANDLES */

/* TASk FUNCTION*/
static void TimerCallback(TimerHandle_t xTimer)
{
	static uint8_t bdt = 0;
	set_LED_BAR(2, 0x00);//sve LEDovke iskljucene
	set_LED_BAR(3, 0xF0);// gornje 4 LEDovke ukljucene
	set_LED_BAR(0, bdt); // ukljucena LED-ovka se pomera od dole ka gore
	bdt <<= 1;
	if (bdt == 0)
		bdt = 1;
}

static uint32_t OnLED_ChangeInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;
	xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &xHigherPTW);
	portYIELD_FROM_ISR(xHigherPTW);
}


void led_bar_tsk(void* pvParameters)
{
	unsigned i;
	uint8_t d;
	while (1)
	{
		xSemaphoreTake(LED_INT_BinarySemaphore, portMAX_DELAY);
		get_LED_BAR(1, &d);
		i = 3;
		do
		{
			i--;
			select_7seg_digit(i);
			set_7seg_digit(hexnum[d % 10]);
			d /= 10;
		} while (i > 0);
	}
}



void main_demo(void)
{
	init_7seg_comm();
	init_LED_comm();// inicijalizacija led bara
	BaseType_t task_created;

	per_TimerHandle = xTimerCreate("Timer", pdMS_TO_TICKS(500), pdTRUE, NULL, TimerCallback);
	xTimerStart(per_TimerHandle, 0);
	/* led bar TASK */
	xTaskCreate(led_bar_tsk, "ST", configMINIMAL_STACK_SIZE, NULL, SERVICE_TASK_PRI, NULL);

	/* ON INPUT CHANGE INTERRUPT HANDLER */
	vPortSetInterruptHandler(portINTERRUPT_SRL_OIC, OnLED_ChangeInterrupt);
	/* Create LED interrapt semaphore */
	LED_INT_BinarySemaphore = xSemaphoreCreateBinary();

	
	vTaskStartScheduler();

	for (;;) {}
}

void vApplicationIdleHook(void) {

	//idleHookCounter++;
}
