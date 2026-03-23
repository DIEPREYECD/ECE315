/*
 * uart_driver.c
 * Created on: July 27, 2021
 * Author: Shyama M. Gandhi
 * Modified by : Antonio Andara
 * Modified on : February 22, 2026
 * TXTRIG-based UART driver
 */

#include "uart_driver.h"
#include "task.h"
#include <portmacro.h>
#include <xil_printf.h>

// -------------------------------------------------
// Global variables
// -------------------------------------------------
XUartPs UART;
XUartPs_Config *Config;
INTC InterruptController;
u32 IntrMask;

// Queues
QueueHandle_t xTxQueue;
QueueHandle_t xRxQueue;

// Interrupt counters
int countRxIrq;
int countTxIrq;
int byteCount;

// -------------------------------------------------
// Interrupt Handler
// -------------------------------------------------
void interruptHandler(void *CallBackRef, u32 event, unsigned int EventData)
{
    u32 isrStatus;

    isrStatus = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_ISR_OFFSET);
    // xil_printf("\nStatus: %d", isrStatus);

    // RX events
    if (isrStatus & (XUARTPS_IXR_RXFULL | XUARTPS_IXR_RXOVR)){
        // xil_printf("\nHandling receive event");
        handleReceiveEvent();
    }

    // TX EMPTY event
    if (isrStatus & XUARTPS_IXR_TXEMPTY){
        handleSentEvent();
    }

    // Clear interrupts
    XUartPs_WriteReg(UART_BASEADDR, XUARTPS_ISR_OFFSET, isrStatus);
}

// -------------------------------------------------
// RX ISR
// -------------------------------------------------
void handleReceiveEvent()
{
    u8 receive_buffer;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Increment each time the ISR fires
    countRxIrq++;

    while (XUartPs_IsReceiveData(UART_BASEADDR)){
        receive_buffer = XUartPs_ReadReg(UART_BASEADDR, UART_FIFO_OFFSET);

        // Counting each byte
        byteCount++;

        xQueueSendFromISR(xRxQueue, &receive_buffer, &xHigherPriorityTaskWoken);

    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// -------------------------------------------------
// TX ISR
// -------------------------------------------------
void handleSentEvent()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    u8 txByte;

    // Count the TX interrupt
    countTxIrq++;
    // xil_printf("\nhSE is writing to serial\n");



    // Fill FIFO while not full and queue has data
    while (!(XUartPs_ReadReg(UART.Config.BaseAddress, XUARTPS_SR_OFFSET) & XUARTPS_SR_TXFULL)){
        // xil_printf("\nhSE read sth in register\n");
        if (xQueueReceiveFromISR(xTxQueue, &txByte, &xHigherPriorityTaskWoken) == pdPASS){
            XUartPs_WriteReg(UART.Config.BaseAddress, XUARTPS_FIFO_OFFSET, txByte);
        } else {
            // No more data → disable TXEMPTY interrupt
            disableTxEmpty();
            break;
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// -------------------------------------------------
// TXEMPTY control
// -------------------------------------------------
void enableTxEmpty()
{
    u32 mask = XUartPs_GetInterruptMask(&UART);
    XUartPs_SetInterruptMask(&UART, mask | XUARTPS_IXR_TXEMPTY);
}

void disableTxEmpty()
{
    u32 mask = XUartPs_GetInterruptMask(&UART);
    XUartPs_SetInterruptMask(&UART, mask & ~XUARTPS_IXR_TXEMPTY);
}

// -------------------------------------------------
// Public API
// -------------------------------------------------
BaseType_t myReceiveData(void)
{
    return (uxQueueMessagesWaiting(xRxQueue) > 0);
}


BaseType_t myTransmitFull(void)
{
    return (uxQueueSpacesAvailable(xTxQueue) == 0);
}


void mySendByte(u8 data)
{
    // xil_printf("mySendByte has been called");
    xQueueSend(xTxQueue, &data, portMAX_DELAY);

    taskENTER_CRITICAL();

    {
        /* 2) If TX FIFO is not full, and TX is idle/empty, "kickstart" by
              writing one byte directly to the FIFO.

           NOTE: On Zynq PS UART, SR has bits like:
             - XUARTPS_SR_TXFULL  : TX FIFO full
             - XUARTPS_SR_TXEMPTY : TX FIFO empty
           If your BSP exposes XUARTPS_SR_TXEMPTY, this is the most direct check.
        */

        u32 sr = XUartPs_ReadReg(UART.Config.BaseAddress, XUARTPS_SR_OFFSET);

        /* Not full? */
        if ((sr & XUARTPS_SR_TXFULL) == 0U)
        {
            /* Idle/empty? (kick only when empty to avoid stuffing extra bytes
               outside the ISR-driven flow) */
            if ((sr & XUARTPS_SR_TXEMPTY) != 0U)
            {
                u8 kickByte;

                /* Pop ONE byte from the queue and write it to FIFO */
                if (xQueueReceive(xTxQueue, &kickByte, 0) == pdPASS)
                {
                    XUartPs_WriteReg(UART.Config.BaseAddress,
                                     XUARTPS_FIFO_OFFSET,
                                     kickByte);
                }
            }
        }

        /* 3) Ensure TXEMPTY interrupt is enabled so ISR drains remaining bytes */
        enableTxEmpty();
    }

    taskEXIT_CRITICAL();

}


u8 myReceiveByte(void)
{
    u8 buf;
    xQueueReceive(xRxQueue, &buf,  portMAX_DELAY);

    return buf;
}


void mySendString(const char* str)
{
    while (*str != '\0') {
        mySendByte((u8)*str);
        str++;
    }
}


// -------------------------------------------------
// Initialization
// -------------------------------------------------
int initializeUART(void)
{
    int Status;

    Config = XUartPs_LookupConfig(UART_DEVICE_ID);
    if (NULL == Config){
        return XST_FAILURE;
    }

    Status = XUartPs_CfgInitialize(&UART, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS){
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

int setupInterruptSystem(INTC *IntcInstancePtr, XUartPs *UartInstancePtr, u16 UartIntrId)
{
    int Status;
    XScuGic_Config *IntcConfig;

    IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
    if (NULL == IntcConfig)
        return XST_FAILURE;

    Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig, IntcConfig->CpuBaseAddress);
    if (Status != XST_SUCCESS)
        return XST_FAILURE;

    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, IntcInstancePtr);

    Status = XScuGic_Connect(IntcInstancePtr, UartIntrId, (Xil_ExceptionHandler)XUartPs_InterruptHandler, (void *)UartInstancePtr);
    if (Status != XST_SUCCESS)
        return XST_FAILURE;

    XScuGic_Enable(IntcInstancePtr, UartIntrId);
    Xil_ExceptionEnable();

    XUartPs_SetHandler(UartInstancePtr, (XUartPs_Handler)interruptHandler, UartInstancePtr);

    // -------------------------------------------------
    // IMPORTANT: RX FIFO trigger level
    // -------------------------------------------------
    XUartPs_SetFifoThreshold(UartInstancePtr, 1);  // interrupt triggers when FIFO <= 1

    // UART interrupt mask, Enable the interrupt when the receive buffer has reached a particular threshold
	IntrMask = XUARTPS_IXR_TOUT | XUARTPS_IXR_RXFULL  |
	           XUARTPS_IXR_RXOVR | XUARTPS_IXR_TXEMPTY;

    XUartPs_SetInterruptMask(UartInstancePtr, IntrMask);
    XUartPs_SetOperMode(UartInstancePtr, XUARTPS_OPER_MODE_NORMAL);

    return XST_SUCCESS;
}


