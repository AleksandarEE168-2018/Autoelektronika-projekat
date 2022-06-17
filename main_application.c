
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
#define TASK_SERIAL_REC_PRI			(3 + tskIDLE_PRIORITY )
#define	SERVICE_TASK_PRI			(1 + tskIDLE_PRIORITY )
#define queue_prioritet			    (4 + tskIDLE_PRIORITY )


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
void prvSerialReceiveTask_1(void* pvParameters);
void QueueReceive_tsk(void* pvParameters);
void SerialSend_Task(void* pvParameters);
void vApplicationIdleHook(void);



/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
const char trigger[] = "SENZOR\n";
const char trigger1[] = "Pozdrav svima manuel brzina 1, nivo kise slaba kisa\n";
uint8_t values[] = "";

unsigned volatile t_point;
unsigned volatile t_point; 

/*struktura koja sadrzi informacije*/
typedef struct {
	uint16_t nivo[3];
	float_t srvr;
	char rezim;
	uint16_t prom;
} poruka_t;

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
	uint8_t  t_point1 = 0;
	while (1)
	{
		if (t_point1 > (uint16_t)((uint16_t)sizeof(trigger1) - (uint16_t)1))
		{
			t_point1 = (uint16_t)0;
		}

		if (send_serial_character((uint8_t)1, (uint8_t)trigger1[t_point1]) != pdTRUE)
		{

		}
		if (send_serial_character((uint8_t)1, (uint8_t)trigger1[t_point1++] + (uint8_t)1) != pdTRUE) 
		{

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
			if (brojac ==  10)
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


static void prvSerialReceiveTask_1(void* pvParameters) 
{
	uint8_t rezim = 'A';
	uint8_t cc;
	uint16_t  cnt = 0, broj[3] = { 0,0,0 }, niz[3] = { 100,10,1 };
	uint16_t broj1 = 0;
	uint16_t uslov = 0, prom = 0;
	uint16_t i, j;
	char tmp0 = '\0', tmp1 = '\0', tmp2 = '\0', tmp3 = '\0';

	while (1) 
	{
		xSemaphoreTake(RXC_BS_1, portMAX_DELAY);
		get_serial_character(COM_CH_1, &cc);

		if (cc == (uint8_t)'C') 
		{
			tmp0 = 'C';
		
		}

		else if ((cc == (uint8_t)'R') && (tmp0 == 'C'))
		{   
			if (rezim == 'A')
			{
				printf("UNICOM1: OK\nUNICOM1: AUTOMATSKI\n");
			}
			if (rezim == 'M')
			{
				printf("UNICOM1: OK\nUNICOM1: MANUELNO\n");
			}

			atoi(values); //string to inthk
			switch (tmp3) 
			{
			case 1:  
				broj[0] = values;
			break;
			case 2:
				broj[1] = values;
			break;
			case 3:
				broj[2] = values;
			break;
		
			}

			tmp0 = 0;
			tmp1 = 0;
			tmp2 = 0;
			tmp3 = 0;
		}


		else if (cc == (uint8_t)'N')
		{
			tmp1 = 'N';
		}

		else if ((cc == (uint8_t)'K') && (tmp1 == 'N'))
		{
			printf("uneo si\n");
			tmp2 = 'K';
		}

		else if (((cc == (uint8_t)49) || (cc == (uint8_t)50) || (cc == (uint8_t)51)) && (tmp1 == 'N') && (tmp2 == 'K'))
		{
			printf("uneo si\n");
			tmp2 = 'K';
			if (cc == (uint8_t)49)
			{	
				tmp3 = 1;
			}
			else if (cc == (uint8_t)50)
			{
				tmp3 = 2;
			}
			else if (cc == (uint8_t)51)
			{
				tmp3 = 3;
			}

		}

		else if (cc == (uint8_t)'A') 
		{
			rezim = 'A';
		}

		else if (cc == (uint8_t)'M') 
		{
			rezim = 'M';
		}

		else
		{
			strcat(values, &cc);
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
	BaseType_t xVraceno;

	init_serial_uplink(COM_CH_0); // inicijalizacija serijske TX na kanalu 0
	init_serial_downlink(COM_CH_0);// inicijalizacija serijske TX na kanalu 0
	init_serial_uplink(COM_CH_1);
	init_serial_downlink(COM_CH_1);
	init_7seg_comm();
	

	/*TASKOVI ZA SENZOR*/
	xTaskCreate(SerialSend_SensorTask, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SENSOR_SEND_PRI, NULL);
	
	xTaskCreate(SerialReceive_SensorTask, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SENSOR_REC_PRI, NULL);
	r_point = 0;

	/* SERIAL TRANSMITTER TASK */
	xTaskCreate(SerialSend_Task, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);

	if (xTaskCreate(QueueReceive_tsk, "Rx", configMINIMAL_STACK_SIZE, NULL, queue_prioritet, NULL) != pdPASS)
		while (1); // task za primanje podataka iz reda
	

	/* SERIAL RECEIVER TASK */
	xVraceno = xTaskCreate(prvSerialReceiveTask_1, "SR1", (uint16_t)((unsigned short)70), NULL, TASK_SERIAL_REC_PRI, NULL);
	if (xVraceno != pdPASS) {
		//
	}

	TBE_BinarySemaphore = xSemaphoreCreateBinary();
	RXC_BinarySemaphore = xSemaphoreCreateBinary();

	myQueue = xQueueCreate(2, sizeof(uint32_t));// kreiramo Queue duzine dva uint32_t

	
	
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);
	vPortSetInterruptHandler(portINTERRUPT_SRL_TBE, prvProcessTBEInterrupt);

	vTaskStartScheduler();

	for (;;) {}

}

void vApplicationIdleHook(void) {

	//idleHookCounter++;
}

