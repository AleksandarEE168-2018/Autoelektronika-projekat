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


/* TASK PRIORITIES */
#define	TASK_SERIAL_SEND_PRI		(2 + tskIDLE_PRIORITY  )
#define TASK_SERIAL_REC_PRI			(3+ tskIDLE_PRIORITY )
#define	SERVICE_TASK_PRI		(1+ tskIDLE_PRIORITY )

/* TASKS: FORWARD DECLARATIONS */

SemaphoreHandle_t RXC_BS_0, RXC_BS_1;
SemaphoreHandle_t TBE_BinarySemaphore;

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH_0 (0)
#define COM_CH_1 (1)

/* TASKS: FORWARD DECLARATIONS */
void led_bar_tsk(void* pvParameters);
void SerialSend_Task(void* pvParameters);

void vApplicationIdleHook(void);

/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
const char trigger[] = "Pozdrav svima manuel brzina 1, nivo kise slaba kisa\n";
unsigned volatile t_point;

/* RECEPTION DATA BUFFER */
#define R_BUF_SIZE (32)
uint8_t r_buffer[R_BUF_SIZE];
unsigned volatile r_point;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const unsigned char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

/* GLOBAL OS-HANDLES */


/* TBE - TRANSMISSION BUFFER EMPTY - INTERRUPT HANDLER */
static uint32_t prvProcessTBEInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;
	xSemaphoreGiveFromISR(TBE_BinarySemaphore, &xHigherPTW);
	portYIELD_FROM_ISR(xHigherPTW);
}


/* TASk FUNCTION*/


void SerialSend_Task(void* pvParameters)
{
	t_point = 0;
	while (1)
	{
		if (t_point > (uint16_t)((uint16_t)sizeof(trigger) - (uint16_t)1)) {
			t_point = (uint16_t)0;
		}

		if (send_serial_character((uint8_t)1, (uint8_t)trigger[t_point]) != pdTRUE) {

		}
		if (send_serial_character((uint8_t)1, (uint8_t)trigger[t_point++] + (uint8_t)1) != pdTRUE) {

		}
		//xSemaphoreTake(TBE_BinarySemaphore, portMAX_DELAY);// kada se koristi predajni interapt
		vTaskDelay(pdMS_TO_TICKS(100)); // kada se koristi vremenski delay 
	}
}



void main_demo(void)
{
	init_serial_uplink(COM_CH_1);
	init_7seg_comm();
	BaseType_t task_created;

	TBE_BinarySemaphore = xSemaphoreCreateBinary();


	/* SERIAL TRANSMITTER TASK */
	xTaskCreate(SerialSend_Task, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);
	
	

	vTaskStartScheduler();

	for (;;) {}
}

void vApplicationIdleHook(void) {

	//idleHookCounter++;
}
