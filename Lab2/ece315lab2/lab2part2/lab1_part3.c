/*
 * Lab 1, Part 2 - Seven-Segment Display & Keypad
 *
 * ECE-315 WINTER 2025 - COMPUTER INTERFACING
 * Created on: February 5, 2021
 * Modified on: July 26, 2023
 * Modified on: January 20, 2025
 * Author(s):  Shyama Gandhi, Antonio Andara Lara
 *
 * Summary:
 * 1) Declare & initialize the 7-seg display (SSD).
 * 2) Use xDelay to alternate between two digits fast enough to prevent flicker.
 * 3) Output pressed keypad digits on both SSD digits: current_key on right, previous_key on left.
 * 4) Print status changes and experiment with xDelay to find minimum flicker-free frequency.
 *
 * Deliverables:
 * - Demonstrate correct display of current and previous keys with no flicker.
 * - Print to the SDK terminal every time that theh variable `status` changes.
 */


// Include FreeRTOS Libraries
#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <portmacro.h>
#include <projdefs.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_intsup.h>
#include <task.h>
#include <queue.h>

// Include xilinx Libraries
#include <xil_types.h>
#include <xparameters.h>
#include <xgpio.h>
#include <xpseudo_asm_gcc.h>
#include <xscugic.h>
#include <xil_exception.h>
#include <xil_printf.h>
#include <sleep.h>
#include <xil_cache.h>

// Other miscellaneous libraries
#include "pmodkypd.h"
#include "rgb_led.h"

#include "xuartps.h"

// Device ID declarations
#define KYPD_DEVICE_ID   	XPAR_GPIO_KYPD_BASEADDR
/*************************** Enter your code here ****************************/
// TODO: Define the seven-segment display (SSD) base address.
#define SSD_ADDRESS         XPAR_GPIO_SSD_BASEADDR

/*****************************************************************************/

// keypad key table
#define DEFAULT_KEYTABLE 	"0FED789C456B123A"

#define UART_BASEADDR 	XPAR_UART1_BASEADDR
#define RX_QUEUE_LEN     256
#define TX_QUEUE_LEN     1024
#define POLL_DELAY_MS     10
#define MAX_CMD_LEN      32


// ======================================================
// Types
// ======================================================
typedef enum {
  CMD_NONE,
  CMD_RGB_BRIGHTNESS = '1',
  CMD_RGB_COLOR = '2',
  CMD_SSD = '3',
} command_type_t;

typedef struct {
    command_type_t type;
    u8 param;
} command_t;


// Declaring the devices
PmodKYPD 	KYPDInst;

/*************************** Enter your code here ****************************/
// TODO: Declare the seven-segment display peripheral here.
XGpio       SSDInst;
XGpio       rgbLedInst;
XGpio       PushBtnInst;

/*****************************************************************************/

// ======================================================
// UART instance
// ======================================================
static XUartPs UartPs;

static void UART_RX_Task(void *pvParameters);
static void UART_TX_Task(void *pvParameters);

// Function prototypes
void InitializeKeypad();
static void vKeypadTask( void *pvParameters );
static void vRgbTask(void *pvParameters);
u32 SSD_decode(u8 key_value, u8 cathode);
static void vButtonsTask(void *pvParameters);
static void vDisplayTask(void *pvParameters);
static void CLI_Task(void *pvParameters);

// QueueHandle_t xQueueKYPD_TO_DSP;

typedef struct {
    u8 prev_key;
    u8 cur_key;
} KYPD_DSP;

// QueueHandle_t xQueueBTN_TO_RGB;
static QueueHandle_t q_rx_byte = NULL;   // uint8_t
static QueueHandle_t q_tx      = NULL;   // char

static QueueHandle_t q_rgb_cmd = NULL;
static QueueHandle_t q_ssd_cmd = NULL;

// ======================================================
// UART helpers
// ======================================================
uint8_t receive_byte(uint8_t *out_byte);
void receive_string(char *buf, size_t buf_len);
static void uart_init(void);
static int uart_poll_rx(uint8_t *b);
static void uart_tx_byte(uint8_t b);
static void parse_and_send(const char* cmd_str);


// ======================================================
// Custom UART functions
// ======================================================
void print_string(const char *str);
void print_new_lines(int count);
void flush_uart(void);

const char *init_message = 
    "Hello wolrd";

int main(void)
{
	int status;

	// Initialize keypad
	InitializeKeypad();

    uart_init();

    // xQueueKYPD_TO_DSP = xQueueCreate(1, sizeof(KYPD_DSP));
    // xQueueBTN_TO_RGB = xQueueCreate(1, sizeof(u32));
    q_rgb_cmd = xQueueCreate(8, sizeof(command_t));
    q_ssd_cmd = xQueueCreate(8, sizeof(command_t));
    q_tx = xQueueCreate(TX_QUEUE_LEN, sizeof(char)); 
    q_rx_byte = xQueueCreate(RX_QUEUE_LEN, sizeof(uint8_t));
    

/*************************** Enter your code here ****************************/
	// TODO: Initialize SSD and set the GPIO direction to output.
    XGpio_Initialize(&SSDInst, SSD_ADDRESS);
    XGpio_Initialize(&rgbLedInst, RGB_LED_BASEADDR);
    XGpio_Initialize(&PushBtnInst, XPAR_GPIO_INPUTS_BASEADDR);

/*****************************************************************************/

	xil_printf("Initialization Complete, System Ready!\n");

    xTaskCreate(CLI_Task, 
                "CLI",     
                2048, 
                init_message, 
                2, 
                NULL);

	xTaskCreate(vKeypadTask,					/* The function that implements the task. */
				"keypad task", 				/* Text name for the task, provided to assist debugging only. */
				configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
				NULL, 						/* The task parameter is not used, so set to NULL. */
				tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
				NULL);

    xTaskCreate(vRgbTask,					/* The function that implements the task. */
				"rgb task", 				/* Text name for the task, provided to assist debugging only. */
				configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
				NULL, 						/* The task parameter is not used, so set to NULL. */
				tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
				NULL);
    
    xTaskCreate(vButtonsTask,					/* The function that implements the task. */
				"buttons task", 				/* Text name for the task, provided to assist debugging only. */
				configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
				NULL, 						/* The task parameter is not used, so set to NULL. */
				tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
				NULL);

    xTaskCreate(vDisplayTask,					/* The function that implements the task. */
				"display task", 				/* Text name for the task, provided to assist debugging only. */
				configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
				NULL, 						/* The task parameter is not used, so set to NULL. */
				tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
				NULL);
    
    xTaskCreate(UART_RX_Task,
                "UART_RX",
                1024, 
                NULL, 
                3, 
                NULL);

    xTaskCreate(UART_TX_Task, 
                "UART_TX", 
                1024, 
                NULL, 
                3, 
                NULL);

    configASSERT(UART_RX_Task);
    configASSERT(UART_TX_Task);
    configASSERT(vKeypadTask);
    configASSERT(vRgbTask);
    configASSERT(vButtonsTask);
    configASSERT(vDisplayTask);
    configASSERT(CLI_Task);
    configASSERT(q_rx_byte);
    // configASSERT(xQueueKYPD_TO_DSP);
    // configASSERT(xQueueBTN_TO_RGB);
    configASSERT(q_tx); // Assertion that it is not null/ is defined

	vTaskStartScheduler();
	while(1);
	return 0;
}

// ======================================================
// UART RX Task
// ======================================================

static void UART_RX_Task(void *pvParameters)
{

  uint8_t byte;

  for (;;){
    if (uart_poll_rx(&byte)){
      xQueueSend(q_rx_byte, &byte, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS)); // Added a delay so that we aren't always busy polling
  }
}

// ======================================================
// UART TX Task
// ======================================================

static void UART_TX_Task(void *pvParameters)
{

  char c;

  for (;;){
    if (xQueueReceive(q_tx, &c, 0) == pdTRUE){ // xQueueReceive can be blocking task when xTicksToWait is set to portMAXDelay and non-blocking when set to 0
      uart_tx_byte((uint8_t)c);
    }
    vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS));
  }
}

// ======================================================
// CLI Task
// ======================================================

static void CLI_Task(void *pvParameters)
{
    command_type_t op = CMD_NONE;
    // crypto_request_t req;
    // crypto_result_t  res;

    uint8_t dummy;

    xil_printf((const char *)pvParameters);
    char cmd_str[MAX_CMD_LEN];


    for (;;){
        xil_printf("\nEnter your option: ");
        receive_string(cmd_str, sizeof(cmd_str));

        if (strlen(cmd_str) == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        parse_and_send(cmd_str);
        vTaskDelay(pdMS_TO_TICKS(1000));
        flush_uart();
    }
}

void parse_and_send(const char *cmd_str) {
    command_t cmd;

    if (strncmp(cmd_str, "LED R", 5) == 0) {
        cmd.type = CMD_RGB_COLOR;
        cmd.param = (u8)RGB_RED;
        xil_printf("Sending RED\r\n");
        xQueueSend(q_rgb_cmd, &cmd, 0);
    }
    else if (strncmp(cmd_str, "LED G", 5) == 0) {
        cmd.type = CMD_RGB_COLOR;
        cmd.param = (u8)RGB_GREEN;
        xil_printf("Sending GREEN\r\n");
        xQueueSend(q_rgb_cmd, &cmd, 0);
    }
    else if (strncmp(cmd_str, "LED B", 5) == 0) {
        cmd.type = CMD_RGB_COLOR;
        cmd.param = (u8)RGB_BLUE;
        xil_printf("Sending BLUE\r\n");
        xQueueSend(q_rgb_cmd, &cmd, 0);
    }
    else if (strncmp(cmd_str, "LED Y", 5) == 0) {
        cmd.type = CMD_RGB_COLOR;
        cmd.param = (u8)RGB_YELLOW;
        xil_printf("Sending YELLOW\r\n");
        xQueueSend(q_rgb_cmd, &cmd, 0);
    }
    else if (strncmp(cmd_str, "LED C", 5) == 0) {
        cmd.type = CMD_RGB_COLOR;
        cmd.param = (u8)RGB_CYAN;
        xil_printf("Sending CYAN\r\n");
        xQueueSend(q_rgb_cmd, &cmd, 0);
    }
    else if (strncmp(cmd_str, "LED M", 5) == 0) {
        cmd.type = CMD_RGB_COLOR;
        cmd.param = (u8)RGB_MAGENTA;
        xil_printf("Sending MAGENTA\r\n");
        xQueueSend(q_rgb_cmd, &cmd, 0);
    }
    else if (strncmp(cmd_str, "LED W", 5) == 0) {
        cmd.type = CMD_RGB_COLOR;
        cmd.param = (u8)RGB_WHITE;
        xil_printf("Sending WHITE\r\n");
        xQueueSend(q_rgb_cmd, &cmd, 0);
    }
    else if (strncmp(cmd_str, "BRT ", 4) == 0) {
        int br = atoi(cmd_str + 4);
        if (br < 1) br = 1;
        if (br > 11) br = 11;

        cmd.type = CMD_RGB_BRIGHTNESS;
        cmd.param = br;
        xil_printf("Sending brightness val: %d\r\n", cmd.param);
        xQueueSend(q_rgb_cmd, &cmd, 0);
    }
    else if ((cmd_str[0] >= '1' && cmd_str[0] <= '9') || 
             (cmd_str[0] >= 'A' && cmd_str[0] <= 'F')) {     
        
        cmd.type = CMD_SSD;
        cmd.param = cmd_str[0]; 
        xil_printf("Sending SSD val: %d\r\n", cmd.param);

        xQueueSend(q_ssd_cmd, &cmd, 0);
    }
    else {
        xil_printf("\nCommand not recognized.");
    }
}


static void vButtonsTask(void *pvParameters) {
    u32 pushed;
    command_t cmd;
    
    while (1) {
        pushed = XGpio_DiscreteRead(&PushBtnInst, 1);
        if (pushed == 8 || pushed == 1) {
            cmd.type = CMD_RGB_BRIGHTNESS;
            if (pushed == 1) {
                cmd.param = (u8)12;
            }
            else {
                cmd.param = (u8)0;
            }

            xQueueSend(q_rgb_cmd, &cmd, 0);
            vTaskDelay(300);
            xil_printf("\nSent pushed: %d\n", pushed);
        }
    }

}

static void vRgbTask(void *pvParameters)
{
    uint8_t color = RGB_WHITE;
	const TickType_t xPeriod = 24;
    const TickType_t xDelay = xPeriod / 2;
    TickType_t xOnDelay = xDelay / 2;
    TickType_t xOffDelay = xDelay - xOnDelay;
    command_t cmd;

    while (1){
        if (xQueueReceive(q_rgb_cmd, &cmd, 0) == pdTRUE) {
            if (cmd.type == CMD_RGB_COLOR) {
                color = cmd.param;
                xil_printf("\n Changed to color to: %d", color);
                vTaskDelay(100);
            }
            else if (cmd.type == CMD_RGB_BRIGHTNESS) {
                if (cmd.param == 0) {
                    xOffDelay = (xOffDelay - 1 < 1) ? 1 : xOffDelay - 1;
                    xOnDelay = (xOnDelay + 1 > 11) ? 11 : xOnDelay + 1;
                }
                else if (cmd.param == 12) {
                    xOnDelay = (xOnDelay - 1 < 1) ? 1 : xOnDelay - 1;
                    xOffDelay = (xOffDelay + 1 > 11) ? 11 : xOffDelay + 1;
                }
                else {
                    xOnDelay = (TickType_t) cmd.param;
                    xOffDelay = xDelay - xOnDelay;
                }

                xil_printf("\n xOff: %d & xOn: %d", xOffDelay, xOnDelay);
                vTaskDelay(100);
            }
            else {
                xil_printf("\nCommand not recognized.");  
            }
        }
        

        XGpio_DiscreteWrite(&rgbLedInst, RGB_CHANNEL, color);
        vTaskDelay(xOnDelay);
        XGpio_DiscreteWrite(&rgbLedInst, RGB_CHANNEL, 0);
        vTaskDelay(xOffDelay);
    }
}

static void vKeypadTask( void *pvParameters )
{
	u16 keystate;
	XStatus status, previous_status = KYPD_NO_KEY;
	u8 new_key, current_key = 'x', previous_key = 'x';

    /*************************** Enter your code here ****************************/
        // TODO: Define a constant of type TickType_t named 'xDelay' and initialize
        //       it with a value of 100.

    /*****************************************************************************/

    xil_printf("Pmod KYPD app started. Press any key on the Keypad.\r\n");
	while (1){
		// Capture state of the keypad
		keystate = KYPD_getKeyStates(&KYPDInst);

		// Determine which single key is pressed, if any
		// if a key is pressed, store the value of the new key in new_key
		status = KYPD_getKeyPressed(&KYPDInst, keystate, &new_key);
		// Print key detect if a new key is pressed or if status has changed
		if (status == KYPD_SINGLE_KEY && previous_status == KYPD_NO_KEY){
			xil_printf("Key Pressed: %c\r\n", (char) new_key);
    /*************************** Enter your code here ****************************/
                // TODO: update value of previous_key and current_key
                previous_key = current_key;
                current_key = new_key;
                command_t val = { .type = CMD_SSD, .param = current_key };

                xQueueSend(q_ssd_cmd, &val, 0);
    /*****************************************************************************/
		} else if (status == KYPD_MULTI_KEY && status != previous_status){
			xil_printf("Error: Multiple keys pressed\r\n");
		}
		
    /*************************** Enter your code here ****************************/
            // TODO: display the value of `status` each time it changes
            if (status != previous_status) {
                xil_printf("Status: %d \n", status);
            }

    /*****************************************************************************/
		previous_status = status;

    /*************************** Enter your code here ****************************/
            /* TODO: Decode the current and previous keys using the `SSD_decode` function.
            * Write each decoded value to the seven-segment display, one at a time,
            * using the `XGpio_DiscreteWrite` function.
            * Add a delay between updates for persistence of vision using `vTaskDelay`.
            */

    /*****************************************************************************/
        vTaskDelay(300);
	}
}

static void vDisplayTask(void *pvParameters) {
    TickType_t xDelay = 11;

    command_t cmd;
    u8 prev;
    u8 cur;
    u32 prev_ssd_val = 'x';
    u32 cur_ssd_val = 'x';
    
    while(1) {

        if (xQueueReceive(q_ssd_cmd, &cmd, 0) == pdTRUE) {
            xil_printf("DisplayTask received: %d\n", cmd.param);
            prev = cur;
            cur = cmd.param;
            cur_ssd_val = SSD_decode(cur, 1);
            prev_ssd_val = SSD_decode(prev, 0);
            
        }

        XGpio_DiscreteWrite(&SSDInst, 1, cur_ssd_val);

        vTaskDelay(xDelay);

        XGpio_DiscreteWrite(&SSDInst, 1, prev_ssd_val);

        vTaskDelay(xDelay);
    }
}


void InitializeKeypad()
{
	KYPD_begin(&KYPDInst, KYPD_DEVICE_ID);
	KYPD_loadKeyTable(&KYPDInst, (u8*) DEFAULT_KEYTABLE);
}

// This function is hard coded to translate key value codes to their binary representation
u32 SSD_decode(u8 key_value, u8 cathode)
{
    u32 result;

	// key_value represents the code of the pressed key
	switch(key_value){ // Handles the coding of the 7-seg display
		case 48: result = 0b00111111; break; // 0
        case 49: result = 0b00110000; break; // 1
        case 50: result = 0b01011011; break; // 2
        case 51: result = 0b01111001; break; // 3
        case 52: result = 0b01110100; break; // 4
        case 53: result = 0b01101101; break; // 5
        case 54: result = 0b01101111; break; // 6
        case 55: result = 0b00111000; break; // 7
        case 56: result = 0b01111111; break; // 8
        case 57: result = 0b01111100; break; // 9
        case 65: result = 0b01111110; break; // A
        case 66: result = 0b01100111; break; // B
        case 67: result = 0b00001111; break; // C
        case 68: result = 0b01110011; break; // D
        case 69: result = 0b01001111; break; // E
        case 70: result = 0b01001110; break; // F
        default: result = 0b00000000; break; // default case - all seven segments are OFF
    }

	// cathode handles which display is active (left or right)
	// by setting the MSB to 1 or 0
    if(cathode==0){
            return result;
    } else {
            return result | 0b10000000;
	}
}

static void uart_init(void)
{
  XUartPs_Config *cfg;

  cfg = XUartPs_LookupConfig(UART_BASEADDR);
  if (!cfg){
    while (1) {}
  }

  if (XUartPs_CfgInitialize(&UartPs, cfg, cfg->BaseAddress) != XST_SUCCESS){
    while (1) {}
  }

  XUartPs_SetBaudRate(&UartPs, 115200);
}

static int uart_poll_rx(uint8_t *b)
{
  if (XUartPs_IsReceiveData(UartPs.Config.BaseAddress)){
    *b = XUartPs_ReadReg(UartPs.Config.BaseAddress, XUARTPS_FIFO_OFFSET);
    return 1;
  }
  return 0;
}

static void uart_tx_byte(uint8_t b)
{
  while (XUartPs_IsTransmitFull(UartPs.Config.BaseAddress)){

  }
  
  XUartPs_WriteReg(UartPs.Config.BaseAddress, XUARTPS_FIFO_OFFSET, b);
}

uint8_t receive_byte(uint8_t *out_byte)
{
    while(1){
        if (xQueueReceive(q_rx_byte, out_byte, 0)!=pdTRUE){            
            vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS));
        } else {
            return *out_byte;
        }
    }
}


void receive_string(char *buf, size_t buf_len)
{
    uint8_t recvd;
    size_t idx = 0;
    buf[0] = '\0';

    while (1){
        if (xQueueReceive(q_rx_byte , &recvd,  0) == pdFALSE) {
            vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS));
        }
        else {
            if (recvd == '\r') {
                break;
            }

            if (idx < buf_len) {
                buf[idx++] = recvd;
                buf[idx] = '\0';
            }
        }
        // vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS));
    }
}


void flush_uart(void)
{
    uint8_t dummy;    
    while (xQueueReceive(q_rx_byte, &dummy, 0) == pdTRUE);
}


void print_string(const char *str)
{
    while (*str) {
        if (xQueueSend(q_tx, str, 0) == pdTRUE) {
            str++;
        }

        // vTaskDelay(pdMS_TO_TICKS(1));
    }   
}

void print_new_lines(int count)
{
    for (int i = 0; i < count; i++){
        xil_printf("\n");
    }
}