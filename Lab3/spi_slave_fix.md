/**
 * SPI Slave Transfer Fix for Lab 3
 * =================================
 *
 * PROBLEM:
 * The master keeps receiving only the first byte of the report repeatedly
 * (e.g., '\n' was printed infinitely, or 'N' when report was changed to start with 'N')
 *
 * ROOT CAUSE:
 * The spiSlaveTransfer function has the wrong order of operations:
 *
 *   Current (WRONG):
 *   void spiSlaveTransfer(const u8 *tx, u8 *rx, int byteCount)
 *   {
 *       spiSlaveWrite(tx, byteCount);  // Put data in TX FIFO
 *       spiSlaveRead(rx, byteCount);  // Wait for RX data
 *   }
 *
 * This doesn't work because:
 * - The slave cannot generate SPI clock - only the master can
 * - When slave calls spiSlaveWrite(), the data goes to TX FIFO but nothing is sent yet
 * - When slave calls spiSlaveRead(), it waits for RX data, but by the time the master
 *   initiates a transfer, there's a race condition and timing issue
 * - The slave ends up receiving stale/incorrect data
 *
 * FIX:
 * Reverse the order - READ first, then WRITE:
 *
 *   Fixed:
 *   void spiSlaveTransfer(const u8 *tx, u8 *rx, int byteCount)
 *   {
 *       spiSlaveRead(rx, byteCount);   // Step 1: Read what master sent (from previous transfer)
 *       spiSlaveWrite(tx, byteCount);  // Step 2: Write what will be sent on NEXT transfer
 *   }
 *
 * WHY THIS WORKS:
 *
 * | Master Action              | Slave Action (Fixed)      | What Master Receives |
 * |----------------------------|---------------------------|----------------------|
 * | Sends '$' (trigger #1)    | Read: gets '$' from RX   | report[0] ('\n')     |
 * |                           | Write: puts report[0]    |                     |
 * | Sends '$' (trigger #2)     | Read: gets '$' from RX   | report[1]            |
 * |                           | Write: puts report[1]    |                     |
 * | Sends '$' (trigger #3)    | Read: gets '$' from RX   | report[2]           |
 * |                           | Write: puts report[2]    |                     |
 * | ...                        | ...                      | ...                 |
 *
 * The key insight: SPI is full-duplex. When master initiates a transfer (generates clock),
 * the data that slave receives is from the PREVIOUS transfer's TX buffer, and the data
 * that master receives is what slave put in TX buffer for THIS transfer.
 *
 * By reading first, the slave processes what came in. By writing second, the slave
 * prepares what will go out on the NEXT master-initiated transfer.
 *
 * LOCATION OF FIX:
 * File: lab3_part1_student/my_spi.c
 * Lines: 115-119
 */

#include "my_spi.h"
#include <stddef.h>
#include "FreeRT.h"
#include "xspips.h"
#include "task.h"


#define SpiPs_SendByte(BaseAddress, Data) \
    XSpiPs_Out32((BaseAddress) + XSPIPS_TXD_OFFSET, (Data))

#define SpiPs_RecvByte(BaseAddress) \
    (u8)XSpiPs_In32((BaseAddress) + XSPIPS_RXD_OFFSET)

static XSpiPs spiMasterInst;
static XSpiPs spiSlaveInst;


/******************************************************************************
/* General SPI functions */
/******************************************************************************/
static void spiWrite(XSpiPs *inst, const u8 *sendBuffer, int byteCount)
{
    int count;
    u32 baseAddr;

    if ((inst == NULL) || (sendBuffer == NULL) || (byteCount <= 0)) {
        return;
    }

    baseAddr = inst->Config.BaseAddress;
    for (count = 0; count < byteCount; count++) {
        SpiPs_SendByte(baseAddr, sendBuffer[count]);
    }
}

static void spiRead(XSpiPs *inst, u8 *recvBuffer, int byteCount)
{
    int count;
    u32 baseAddr;
    u32 statusReg;

    if ((inst == NULL) || (recvBuffer == NULL) || (byteCount <= 0)) {
        return;
    }

    baseAddr = inst->Config.BaseAddress;
    do {
        statusReg = XSpiPs_ReadReg(baseAddr, XSPIPS_SR_OFFSET);
    } while (!(statusReg & XSPIPS_IXR_RXNEMPTY_MASK));

    for (count = 0; count < byteCount; count++) {
        recvBuffer[count] = SpiPs_RecvByte(baseAddr);
    }
}


/******************************************************************************
/* SPI Master specific Functions*/
/******************************************************************************/
void spiMasterWrite(const u8 *tx, int byteCount)
{
    // TODO 4: write the body for this function
    if ((tx == NULL) || (byteCount <= 0)) {
        return;
    }
    spiWrite(&spiMasterInst, tx, byteCount);
}


void spiMasterRead(u8 *rx, int byteCount)
{
    // TODO 5: write the body for this function
    if ((rx == NULL) || (byteCount <= 0)) {
        return;
    }
    spiRead(&spiMasterInst, rx, byteCount);
}


void spiMasterTransfer(const u8 *tx, u8 *rx, int byteCount)
{
    // TODO 6: write the body for this function using spiMasterWrite and spiMasterRead
    if ((tx == NULL) || (rx == NULL) || (byteCount <= 0)) {
        return;
    }

    spiMasterWrite(tx, byteCount);
    spiMasterRead(rx, byteCount);
}


/******************************************************************************
/* SPI Sub specific Functions */
/******************************************************************************/
void spiSlaveWrite(const u8 *tx, int byteCount)
{
    // TODO 7: write the body for this function
    if ((tx == NULL) || (byteCount <= 0)) {
        return;
    }
    spiWrite(&spiSlaveInst, tx, byteCount);
}


void spiSlaveRead(u8 *rx, int byteCount)
{
    // TODO 8: write the body for this function
    if ((rx == NULL) || (byteCount <= 0)) {
        return;
    }
    spiRead(&spiSlaveInst, rx, byteCount);
}


/******************************************************************************
/* FIXED: spiSlaveTransfer - Read first, then write
/******************************************************************************
/* NOTE: The order matters! For SPI slave, you must READ first, then WRITE
 * because:
 * 1. The master generates the SPI clock
 * 2. When slave reads, it gets data from the master's PREVIOUS transmission
 * 3. When slave writes, it prepares data for the master's NEXT transmission
 *
 * This is the key fix for the infinite newline bug!
 */
void spiSlaveTransfer(const u8 *tx, u8 *rx, int byteCount)
{
    // FIXED ORDER:
    // Step 1: Read what the master sent (from previous transfer)
    spiSlaveRead(rx, byteCount);
    // Step 2: Write what will be sent on next master-initiated transfer
    spiSlaveWrite(tx, byteCount);
}


int spiInit(u32 masterDeviceId, u32 slaveDeviceId)
{
    int status;
    XSpiPs_Config *masterCfg;
    XSpiPs_Config *slaveCfg;

    masterCfg = XSpiPs_LookupConfig(masterDeviceId);
    if (masterCfg == NULL) {
        return XST_FAILURE;
    }

    status = XSpiPs_CfgInitialize(&spiMasterInst, masterCfg, masterCfg->BaseAddress);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    slaveCfg = XSpiPs_LookupConfig(slaveDeviceId);
    if (slaveCfg == NULL) {
        return XST_FAILURE;
    }

    status = XSpiPs_CfgInitialize(&spiSlaveInst, slaveCfg, slaveCfg->BaseAddress);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    status = XSpiPs_SetOptions(&spiMasterInst,
        (XSPIPS_CR_CPHA_MASK) | (XSPIPS_MASTER_OPTION) | (XSPIPS_CR_CPOL_MASK));
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    status = XSpiPs_SetOptions(&spiSlaveInst, (XSPIPS_CR_CPHA_MASK) | (XSPIPS_CR_CPOL_MASK));
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}
