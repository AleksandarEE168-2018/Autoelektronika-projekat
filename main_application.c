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

#define	TASK_SERIAL_SEND_PRI		(2 + tskIDLE_PRIORITY  )
#define TASK_SERIAL_REC_PRI			(3+ tskIDLE_PRIORITY )
#define	SERVICE_TASK_PRI		(1+ tskIDLE_PRIORITY )
#define stack_size configMINIMAL_STACK_SIZE;

typedef float float_t;
typedef double double_t;
typedef unsigned short us_t;


/* TASKS: FORWARD DECLARATIONS */
static void prvSerialReceiveTask_0(void* pvParameters);
static void prvSerialReceiveTask_1(void* pvParameters);
static void SerialSend_Task(void* pvParameters);
static void serialsend1_tsk(void* pvParameters);
static void set_led_Task(void* pvParameters);
//static void set_sev_seg_Task(void* pvParameters);
extern void main_demo(void);



static QueueHandle_t sk_q1, sk_q2, sk_q3;
static SemaphoreHandle_t RXC_BS_0, RXC_BS_1, led_sem;
static BaseType_t my_tsk;

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH_0 (0)
#define COM_CH_1 (1)

/* TRASNMISSION DATA */
static char trigger[20] = "POZDRAV\n";
static char trigger1[30] = "";
static char ispisr = 'A';
static float_t minn = (float_t)0;
static float_t maxx = (float_t)1000;
char values[3] = "";
uint16_t values_int = 0;
char cc_to_str[2] = { '0', '\0' };
uint16_t  ispisn[3] = { 0,0,0 };
uint8_t rezim = 'A';

typedef struct {
	uint16_t nivo[3];
	float_t srvr;
	char rezim;
	uint16_t prom;
} poruka_s;

static uint16_t volatile t_point;
static uint16_t volatile t_point1;

/* RECEPTION DATA BUFFER */
#define R_BUF_SIZE (32)
static uint16_t r_buffer[R_BUF_SIZE];

static uint16_t volatile r_point;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const uint16_t hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
	0x15, 0x6d };


static SemaphoreHandle_t  TBE_BinarySemaphore;
static SemaphoreHandle_t RXC_BinarySemaphore;
static SemaphoreHandle_t LED_INT_BinarySemaphore;
static TimerHandle_t tH1;
static TimerHandle_t checkIdleCounterTimer;



static uint32_t OnLED_ChangeInterrupt() {
	// Ovo se desi kad neko pritisne dugme na LED Bar tasterima
	BaseType_t xHigherPTW = pdFALSE;

	if (xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &xHigherPTW) != pdTRUE) {

	}

	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

/* TBE - TRANSMISSION BUFFER EMPTY - INTERRUPT HANDLER */
static uint32_t prvProcessTBEInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;
	if (xSemaphoreGiveFromISR(TBE_BinarySemaphore, &xHigherPTW) != pdTRUE) {

	}

	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}


/* RXC - RECEPTION COMPLETE - INTERRUPT HANDLER */
static uint32_t prvProcessRXCInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;

	if (get_RXC_status((uint8_t)0) != 0) {
		if (xSemaphoreGiveFromISR(RXC_BS_0, &xHigherPTW) != pdTRUE) {

		}
	}

	if (get_RXC_status((uint8_t)1) != 0) {
		if (xSemaphoreGiveFromISR(RXC_BS_1, &xHigherPTW) != pdTRUE) {

		}
	}
	if (get_RXC_status((uint8_t)2) != 0) {
		if (xSemaphoreGiveFromISR(led_sem, &xHigherPTW) != pdTRUE) {

		}
	}

	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

static void prvSerialReceiveTask_0(void* pvParameters) {
	poruka_s poruka;
	uint8_t ss;
	float_t tmp = (float_t)0;
	uint16_t cnt = 0;

	for (;;) {
		if (xSemaphoreTake(RXC_BS_0, portMAX_DELAY) != pdTRUE) {

		}
		if (get_serial_character(COM_CH_0, &ss) != pdTRUE) {

		}
		if (ss == (uint16_t)0x00) {

		}
		else if (ss == (uint16_t)0xff) {

		}
		else {
			cnt++;
			tmp += (float_t)ss;
			if (cnt == (uint16_t)10) {
				tmp /= (float_t)10;
				tmp *= (float_t)100;
				poruka.srvr = tmp;
				if (minn > tmp) {
					minn = tmp;
				}
				if (maxx < tmp) {
					maxx = tmp;
				}
				cnt = 0;
				printf("UNICOM0: trenutno stanje senzora: %.2f\n", poruka.srvr / 10);
				tmp = (float_t)0;
				if (xQueueSend(sk_q1, &poruka.srvr, 0) != pdTRUE) {

				}
			}
		}
	}
}

static void prvSerialReceiveTask_1(void* pvParameters) {


	uint8_t cc;



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
			ispisr = rezim;
			values_int = (uint16_t)atoi(values); //string to inth
			printf(" * Uneo si %d * ", values_int);
			switch (tmp3)
			{
			case 1:

				ispisn[0] = values_int;
				break;
			case 2:

				ispisn[1] = values_int;
				break;
			case 3:

				ispisn[2] = values_int;
				break;

			}

			printf("%d, %d, %d ", ispisn[0], ispisn[1], ispisn[2]);
			tmp0 = 0;
			tmp1 = 0;
			tmp2 = 0;
			tmp3 = 0;
			values[0] = '\0';
		}


		else if (cc == (uint8_t)'N')
		{
			tmp1 = 'N';
		}

		else if ((cc == (uint8_t)'K') && (tmp1 == 'N'))
		{
			tmp2 = 'K';
		}

		else if (((cc == (uint8_t)49) || (cc == (uint8_t)50) || (cc == (uint8_t)51)) && (tmp1 == 'N') && (tmp2 == 'K'))
		{
			tmp1 = '0';
			tmp2 = '0';

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
			cc_to_str[0] = cc;
			strcat(values, cc_to_str);
		}

	}
}

static void set_led_Task(void* pvParameters) {
	poruka_s poruka;
	unsigned i;
	uint8_t d;
	set_LED_BAR(1, 0x00);
	while (1) {
		xSemaphoreTake(led_sem, portMAX_DELAY);
		get_LED_BAR(0, &d);  //ocitavanje prvog stubca
		//printf("vrednost led bara: %d", d);

		if (rezim == 'M') {
			sprintf(trigger1, "0 ");

			if (d == 1)
			{
				printf("prva brzina\n");
				set_LED_BAR(1, 0x01);
				select_7seg_digit(1);
				set_7seg_digit(hexnum[1]);
				select_7seg_digit(2);
				set_7seg_digit(hexnum[1]);
			}
			else if (d == 3)
			{
				printf("druga brzina\n");
				set_LED_BAR(1, 0x01);
				select_7seg_digit(1);
				set_7seg_digit(hexnum[2]);
				select_7seg_digit(2);
				set_7seg_digit(hexnum[1]);
			}
			else if (d == 7)
			{
				printf("treca brzina\n");
				set_LED_BAR(1, 0x01);
				select_7seg_digit(1);
				set_7seg_digit(hexnum[3]);
				select_7seg_digit(2);
				set_7seg_digit(hexnum[1]);
			}
			else
			{
				printf("brisac iskljucen\n");
				set_LED_BAR(1, 0x00);
				select_7seg_digit(1);
				set_7seg_digit(hexnum[0]);
				select_7seg_digit(2);
				set_7seg_digit(hexnum[0]);
			}
		}
		else if (rezim == 'A')
		{
			if (xQueueReceive(sk_q1, &poruka.srvr, portMAX_DELAY) != pdTRUE) {

			}

			//sprintf(trigger1, "%.2f ", poruka.srvr);
			if ((poruka.srvr) / 10 < ((float_t)ispisn[0])) {
				//printf("Auto,prva brzina\n");
				sprintf(trigger1, "Prva brzina\n ");
			}
			else if ((poruka.srvr) / 10 < ((float_t)ispisn[1]))
			{
				//printf("Auto,druga brzina\n");
				sprintf(trigger1, "Druga brzina\n ");
			}
			else if ((poruka.srvr) / 10 < ((float_t)ispisn[2]))
			{
				//	printf("Auto,treca brzina\n");
				sprintf(trigger1, "Treca brzina\n ");
			}
		}
	}

	/*
	poruka_s poruka;
	uint8_t tmp = 0, tmp2 = 0, tmp1;
	uint16_t prom;
	for (;;)
	{

		if (xQueueReceive(sk_q1, &poruka.srvr, portMAX_DELAY) != pdTRUE) {

		}

		sprintf(trigger1, "%.2f ", poruka.srvr);


		if ((poruka.srvr) < ((float_t)ispisn[0])) {
			vTaskDelay(200);
			if (set_LED_BAR((uint8_t)2, (uint8_t)0x00) != pdTRUE) {

			}
			if (set_LED_BAR((uint8_t)1, (uint8_t)0x00) != pdTRUE) {

			}
		}
		else {
			if (set_LED_BAR((uint8_t)1, (uint8_t)128) != pdTRUE) {

			}
		}

		if (ispisr == 'M') {
			strcat(trigger1, "Manuelno\n");
			if ((uint8_t)get_LED_BAR((uint8_t)0, (uint8_t*)&tmp2) != ((uint8_t)0)) {
				printf("greska");
			}
			else
			{
				if (select_7seg_digit((uint8_t)0) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[0]) != pdTRUE) {

				}
				if (set_LED_BAR((uint8_t)2, (uint8_t)tmp2) != pdTRUE) {

				}
				if ((tmp2) == ((uint8_t)0)) {
					if (select_7seg_digit((uint8_t)2) != pdTRUE) {

					}
					if (set_7seg_digit((uint8_t)hexnum[0]) != pdTRUE) {

					}
					if (set_LED_BAR((uint8_t)1, (uint8_t)0) != pdTRUE) {

					}
					strcat(trigger1, "Ne Rade ");
				}
				else if ((tmp2) == ((uint8_t)1)) {
					if (select_7seg_digit((uint8_t)2) != pdTRUE) {

					}
					if (set_LED_BAR((uint8_t)1, (uint8_t)128) != pdTRUE) {

					}
					if (set_7seg_digit((uint8_t)hexnum[1]) != pdTRUE) {

					}
					strcat(trigger1, "Rade ");
				}
				else if ((tmp2) == ((uint8_t)3)) {
					if (select_7seg_digit((uint8_t)2) != pdTRUE) {

					}
					if (set_7seg_digit((uint8_t)hexnum[2]) != pdTRUE) {

					}
					if (set_LED_BAR((uint8_t)1, (uint8_t)128) != pdTRUE) {

					}
					strcat(trigger1, "Rade ");
				}
				else if ((tmp2) == ((uint8_t)7)) {
					if (select_7seg_digit((uint8_t)2) != pdTRUE) {
					}
					if (set_LED_BAR((uint8_t)1, (uint8_t)128) != pdTRUE) {

					}
					if (set_7seg_digit((uint8_t)hexnum[3]) != pdTRUE) {

					}
					strcat(trigger1, "Rade ");
				}
				else {
					//
				}
			}
		}

		else {
			strcat(trigger1, "Automatski\n");
			if (select_7seg_digit((uint8_t)0) != pdTRUE) {

			}
			if (set_7seg_digit((uint8_t)hexnum[1]) != pdTRUE) {

			}
			if ((poruka.srvr) < ((float_t)ispisn[0])) {
				strcat(trigger1, "Ne Rade ");
			}
			else if (((poruka.srvr) >= ((float_t)ispisn[0])) && ((poruka.srvr) <= ((float_t)ispisn[1]))) {
				if (set_LED_BAR((uint8_t)2, (uint8_t)1) != pdTRUE) {

				}
				strcat(trigger1, "Rade ");
			}
			else if (((poruka.srvr) >= ((float_t)ispisn[1])) && ((poruka.srvr) <= ((float_t)ispisn[2]))) {
				if (set_LED_BAR((uint8_t)2, (uint8_t)3) != pdTRUE) {

				}
				strcat(trigger1, "Rade ");
			}
			else if (((poruka.srvr) >= ((float_t)ispisn[2])) && (((float_t)poruka.srvr) <= ((float_t)1000))) {
				if (set_LED_BAR((uint8_t)2, (uint8_t)7) != pdTRUE) {

				}
				strcat(trigger1, "Rade ");
			}
			else {
				//
			}
			if ((uint8_t)get_LED_BAR((uint8_t)2, &tmp1) != (uint8_t)0) {
				printf("greska");
			}
			else
			{
				if ((tmp1) == ((uint8_t)0)) {
					if (select_7seg_digit((uint8_t)2) != pdTRUE) {

					}
					if (set_7seg_digit((uint8_t)hexnum[0]) != pdTRUE) {

					}
				}
				else if ((tmp1) == ((uint8_t)1)) {
					if (select_7seg_digit((uint8_t)2) != pdTRUE) {

					}
					if (set_7seg_digit((uint8_t)hexnum[1]) != pdTRUE) {

					}
				}
				else if ((tmp1) == ((uint8_t)3)) {
					if (select_7seg_digit((uint8_t)2) != pdTRUE) {

					}
					if (set_7seg_digit((uint8_t)hexnum[2]) != pdTRUE) {

					}
				}
				else if ((tmp1) == ((uint8_t)7)) {
					if (select_7seg_digit((uint8_t)2) != pdTRUE) {

					}
					if (set_7seg_digit((uint8_t)hexnum[3]) != pdTRUE) {

					}
				}
				else {
					//
				}
			}
		}
	}
	*/
}

/*static void set_sev_seg_Task(void* pvParameters) {
	poruka_s poruka;
	uint8_t d;
	uint16_t prom;

	for (;;) {
		if (xQueueReceive(sk_q1, &poruka.srvr, portMAX_DELAY) != pdTRUE) {

		}

		if (get_LED_BAR(3, &d) != 0) {
			printf("Greska\n");
		}
		else {
			if (((d) & ((uint8_t)(0x01))) != (uint8_t)0) {
				printf("minimalna vrednost %.2f\n", minn);
				prom = (uint16_t)minn;
				if (select_7seg_digit((uint8_t)4) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)(prom / (uint16_t)1000)]) != pdTRUE) {

				}
				prom %= (uint16_t)1000;
				if (select_7seg_digit((uint8_t)5) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)(prom / (uint16_t)100)]) != pdTRUE) {

				}
				prom %= (uint16_t)100;
				if (select_7seg_digit((uint8_t)6) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)(prom / (uint16_t)10)]) != pdTRUE) {

				}
				prom %= (uint16_t)10;
				if (select_7seg_digit((uint8_t)7) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)prom]) != pdTRUE) {

				}
				prom = (uint16_t)0;

			}

			if (((d) & ((uint8_t)(0x02))) != (uint8_t)0) {
				printf("trenutna srvr %.2f\n", poruka.srvr);
				prom = (uint16_t)poruka.srvr;
				if (select_7seg_digit((uint8_t)4) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)(prom / (uint16_t)1000)]) != pdTRUE) {

				}
				prom %= (uint16_t)1000;
				if (select_7seg_digit((uint8_t)5) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)(prom / (uint16_t)100)]) != pdTRUE) {

				}
				prom %= (uint16_t)100;
				if (select_7seg_digit((uint8_t)6) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)(prom / (uint16_t)10)]) != pdTRUE) {

				}
				prom %= (uint16_t)10;
				if (select_7seg_digit((uint8_t)7) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)prom]) != pdTRUE) {

				}
				prom = (uint16_t)0;
			}

			if (((d) & ((uint8_t)(0x04))) != (uint8_t)0) {
				printf("max vrednost %.2f\n", maxx);
				prom = (uint16_t)maxx;
				if (select_7seg_digit((uint8_t)4) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)(prom / (uint16_t)1000)]) != pdTRUE) {

				}
				prom %= (uint16_t)1000;
				if (select_7seg_digit((uint8_t)5) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)(prom / (uint16_t)100)]) != pdTRUE) {

				}
				prom %= (uint16_t)100;
				if (select_7seg_digit((uint8_t)6) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)(prom / (uint16_t)10)]) != pdTRUE) {

				}
				prom %= (uint16_t)10;
				if (select_7seg_digit((uint8_t)7) != pdTRUE) {

				}
				if (set_7seg_digit((uint8_t)hexnum[(uint8_t)prom]) != pdTRUE) {

				}
				prom = (uint16_t)0;
			}
		}
	}
}*/

static void serialsend1_tsk(void* pvParameters) {
	t_point1 = 0;
	for (;;) {
		if (t_point1 > (uint16_t)((uint16_t)sizeof(trigger1) - (uint16_t)1)) {
			t_point1 = (uint16_t)0;
		}

		if (send_serial_character((uint8_t)1, (uint8_t)trigger1[t_point1]) != pdTRUE) {

		}
		if (send_serial_character((uint8_t)1, (uint8_t)trigger1[t_point1++] + (uint8_t)1) != pdTRUE) {

		}
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

static void SerialSend_Task(void* pvParameters)
{
	t_point = 0;
	for (;;)
	{
		if (t_point > (uint16_t)((uint16_t)sizeof(trigger) - (uint16_t)1)) {
			t_point = (uint16_t)0;
		}

		if (send_serial_character((uint8_t)0, (uint8_t)trigger[t_point]) != pdTRUE) {

		}
		if (send_serial_character((uint8_t)0, (uint8_t)trigger[t_point++] + (uint8_t)1) != pdTRUE) {

		}
		//xSemaphoreTake(TBE_BinarySemaphore, portMAX_DELAY);// kada se koristi predajni interapt
		vTaskDelay(pdMS_TO_TICKS(200)); // kada se koristi vremenski delay }
	}
}

extern void main_demo(void)
{
	if (init_7seg_comm() != pdTRUE) {
	}
	if (init_LED_comm() != pdTRUE) {

	}
	if (init_serial_uplink(COM_CH_0) != pdTRUE) {

	}
	if (init_serial_downlink(COM_CH_0) != pdTRUE) {

	}
	if (init_serial_uplink(COM_CH_1) != pdTRUE) {

	}
	if (init_serial_downlink(COM_CH_1) != pdTRUE) {

	}

	//interrupts
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);
	vPortSetInterruptHandler(portINTERRUPT_SRL_TBE, prvProcessTBEInterrupt);

	RXC_BS_0 = xSemaphoreCreateBinary();
	RXC_BS_1 = xSemaphoreCreateBinary();
	led_sem = xSemaphoreCreateBinary();

	/* Create LED interrapt semaphore */
	LED_INT_BinarySemaphore = xSemaphoreCreateBinary();
	RXC_BinarySemaphore = xSemaphoreCreateBinary();
	TBE_BinarySemaphore = xSemaphoreCreateBinary();

	sk_q1 = xQueueCreate(10, sizeof(sk_q1));
	if (sk_q1 == NULL) {

	}
	sk_q2 = xQueueCreate(10, sizeof(sk_q2));
	if (sk_q2 == NULL) {

	}
	sk_q3 = xQueueCreate(10, sizeof(sk_q3));
	if (sk_q3 == NULL) {

	}

	BaseType_t xVraceno;

	/* SERIAL RECEIVER TASK */
	xVraceno = xTaskCreate(prvSerialReceiveTask_0, "SR0", (uint16_t)((us_t)70), NULL, TASK_SERIAL_REC_PRI, NULL);
	if (xVraceno != pdPASS) {
		//
	}

	/* SERIAL RECEIVER TASK */
	xVraceno = xTaskCreate(prvSerialReceiveTask_1, "SR1", (uint16_t)((us_t)70), NULL, TASK_SERIAL_REC_PRI, NULL);
	if (xVraceno != pdPASS) {
		//
	}

	/* SERIAL TRANSMITTER TASK */
	xVraceno = xTaskCreate(SerialSend_Task, "STx", (uint16_t)((us_t)70), NULL, TASK_SERIAL_SEND_PRI, NULL);
	if (xVraceno != pdPASS) {
		//
	} 


	xVraceno = xTaskCreate(serialsend1_tsk, "stx1", (uint16_t)((us_t)70), NULL, TASK_SERIAL_SEND_PRI, NULL);
	if (xVraceno != pdPASS) {
		//
	}

	/*set led*/
	xVraceno = xTaskCreate(set_led_Task, "Stld", (uint16_t)((us_t)70), NULL, TASK_SERIAL_SEND_PRI, NULL);
	if (xVraceno != pdPASS) {
		//
	}



	//xVraceno = xTaskCreate(set_sev_seg_Task, "Stld", (uint16_t)((us_t)70), NULL, TASK_SERIAL_SEND_PRI, NULL);
	//if (xVraceno != pdPASS) {
		//
	//}


	vTaskStartScheduler();

	for (;;) {
		//
	}
}




