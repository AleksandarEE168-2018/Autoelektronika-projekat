
/* Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/* Hardware simulator utility functions */
#include "HW_access.h"


/* TASK PRIORITIES */
#define TASK_SERIAL_SEND_PRI		(2 + tskIDLE_PRIORITY )
#define	TASK_SENSOR_SEND_PRI		(2 + tskIDLE_PRIORITY )
#define TASK_SENSOR_REC_PRI			(3 + tskIDLE_PRIORITY )
#define	SERVICE_TASK_PRI			(1 + tskIDLE_PRIORITY )
#define queue_prioritet			    (5 + tskIDLE_PRIORITY )
#define TASK_MODE_PRI               (4 + tskIDLE_PRIORITY )


/*SEMAPHORE HANDLE*/
SemaphoreHandle_t RXC_BinarySemaphore;
SemaphoreHandle_t RXC_BS_0, RXC_BS_1;
SemaphoreHandle_t TBE_BinarySemaphore;

QueueHandle_t myQueue = NULL;


/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH_0 (0)
#define COM_CH_1 (1)


/* TASKS: FORWARD DECLARATIONS */
void SerialSend_SensorTask(void* pvParameters);
void SerialRecive_SensorTask(void* pvParameters);
void QueueReceive_tsk(void* pvParameters);
void SerialSend_Task(void* pvParameters);
void vApplicationIdleHook(void);
void SerialRecive_ModeTask(void* pvParameters);



/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
const char trigger[] = "SENZOR\n";
const char trigger1[] = "Pozdrav svima manuel brzina 1, nivo kise slaba kisa\n";

unsigned volatile t_point;
unsigned volatile t_point1;

/* RECEPTION DATA BUFFER */
#define R_BUF_SIZE (32)
uint8_t r_buffer[R_BUF_SIZE];
unsigned volatile r_point;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const unsigned char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

/* GLOBAL OS-HANDLES */
static float_t MIN = 0;
static float_t MAX = 1000;
float_t sr_vrednost = 0;

/* RXC - RECEPTION COMPLETE - INTERRUPT HANDLER */
static uint32_t prvProcessRXCInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;
	xSemaphoreGiveFromISR(RXC_BinarySemaphore, &xHigherPTW);
	portYIELD_FROM_ISR(xHigherPTW);
}



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
	t_point1 = 0;
	while (1)
	{
		if (t_point1 > (uint16_t)((uint16_t)sizeof(trigger1) - (uint16_t)1)) {
			t_point1 = (uint16_t)0;
		}

		if (send_serial_character((uint8_t)1, (uint8_t)trigger1[t_point1]) != pdTRUE) {

		}
		if (send_serial_character((uint8_t)1, (uint8_t)trigger1[t_point1++] + (uint8_t)1) != pdTRUE) {

		}
		//xSemaphoreTake(TBE_BinarySemaphore, portMAX_DELAY);// kada se koristi predajni interapt
		vTaskDelay(pdMS_TO_TICKS(200)); // kada se koristi vremenski delay 

	}
}





void SerialSend_SensorTask(void* pvParameters)
{
	t_point = 0;
	while (1)
	{
		if (t_point > (sizeof(trigger) - 1))
			t_point = 0;
		send_serial_character(COM_CH_0, trigger[t_point++]);
		//xSemaphoreTake(TBE_BinarySemaphore, portMAX_DELAY);// kada se koristi predajni interapt
		vTaskDelay(pdMS_TO_TICKS(200)); // kada se koristi vremenski delay }
	}
}


void SerialReceive_SensorTask(void* pvParameters)
{
	float_t poruka = 0;
	uint8_t cc = 0;
	static uint8_t brojac = 0;
	float_t tmp = 0;
	
	while (1)
	{
		xSemaphoreTake(RXC_BinarySemaphore, portMAX_DELAY);// ceka na serijski prijemni interapt
		get_serial_character(COM_CH_0, &cc);//ucitava primljeni karakter u promenjivu cc
		//printf("primio karakter: %u\n", (unsigned)cc);// prikazuje primljeni karakter u cmd prompt

		if (cc != 0x00 && cc != 0xff)
		{
			brojac++;
			tmp += (float_t)cc;
			if (brojac == 10)
			{
				tmp /= 10;
				tmp *= 100;
				sr_vrednost = tmp;
				if (sr_vrednost < MIN)
				{
					sr_vrednost = MIN;
				}
				if (sr_vrednost > MAX)
				{
					sr_vrednost = MAX;
				}
				brojac = 0;
				tmp = 0;

				printf("Vrednosti senzora: %.2f\n", sr_vrednost);
				xQueueSend(myQueue, &sr_vrednost, 0U); // salje podatak iz taska u red myQueue
				
			}
			
		}
			
	}
}



static void QueueReceive_tsk(void* pvParameters)
{
	float_t ulReceivedValue;
	uint16_t a_num = 0;
	uint16_t b_num = 0;
	for (;; )
	{

		xQueueReceive(myQueue, &ulReceivedValue, portMAX_DELAY); // task ceka u blokiranom stanju
			//dok ne dobije podatak iz Queue


		printf("Prosledjena vrednost sa senzora: %.2f\n", ulReceivedValue);
	}
}





void main_demo(void)
{
	init_serial_uplink(COM_CH_0); // inicijalizacija serijske TX na kanalu 0
	init_serial_downlink(COM_CH_0);// inicijalizacija serijske TX na kanalu 0
	init_serial_uplink(COM_CH_1);
	init_7seg_comm();
	

	/*TASKOVI ZA SENZOR*/
	xTaskCreate(SerialSend_SensorTask, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SENSOR_SEND_PRI, NULL);
	
	xTaskCreate(SerialReceive_SensorTask, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SENSOR_REC_PRI, NULL);
	r_point = 0;

	/* SERIAL TRANSMITTER TASK */
	xTaskCreate(SerialSend_Task, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);

	xTaskCreate(SerialRecive_ModeTask, "SRMx", configMINIMAL_STACK_SIZE, NULL, TASK_MODE_PRI, NULL);

	if (xTaskCreate(QueueReceive_tsk, "Rx", configMINIMAL_STACK_SIZE, NULL, queue_prioritet, NULL) != pdPASS)
		while (1); // task za primanje podataka iz reda
	
	TBE_BinarySemaphore = xSemaphoreCreateBinary();
	RXC_BinarySemaphore = xSemaphoreCreateBinary();

	myQueue = xQueueCreate(2, sizeof(uint32_t));// kreiramo Queue duzine dva uint32_t

	
	
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);

	vTaskStartScheduler();

	for (;;) {}

}

void vApplicationIdleHook(void) {

	//idleHookCounter++;
}

