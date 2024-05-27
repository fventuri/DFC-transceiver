//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "cyu3system.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3gpif.h"
#include "cyu3utils.h"

#include "dac_modes.h"
#include "app.h"
#include "dfc_modes.h"
#include "gpif.h"
#include "gpio.h"
#include "i2c.h"
#include "uart.h"

/* global variables for this file */
static CyU3PDmaMultiChannel glDmaChHandle;
static unsigned long glDMAProdEvent = 0;
static unsigned long glDMAProdEventPrevIter = 0;

/* internal functions at the end of this file */
static void CyFxApplnEpCallback(CyU3PUsbEpEvtType evtype, CyU3PUSBSpeed_t usbSpeed, uint8_t epNum);
static void GpifToUsbDmaCallback(CyU3PDmaMultiChannel *chHandle, CyU3PDmaCbType_t type, CyU3PDmaCBInput_t *input);


void DacModesInit(CyBool_t is32Wide) {
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PReturnStatus_t stat;

    CyU3PMemSet ((uint8_t *)&io_cfg, 0, sizeof (io_cfg));
    io_cfg.isDQ32Bit = is32Wide;
    io_cfg.useUart   = CyTrue;
    io_cfg.useI2C    = CyTrue;
    io_cfg.useI2S    = CyFalse;
    io_cfg.useSpi    = CyFalse;
    io_cfg.lppMode   = is32Wide ? CY_U3P_IO_MATRIX_LPP_DEFAULT :
                                  CY_U3P_IO_MATRIX_LPP_UART_ONLY;
    io_cfg.s0Mode    = CY_U3P_SPORT_INACTIVE;
    io_cfg.s1Mode    = CY_U3P_SPORT_INACTIVE;

    /* No GPIOs are enabled. */
    io_cfg.gpioSimpleEn[0]  = 0;
    io_cfg.gpioSimpleEn[1]  = 0;
    io_cfg.gpioComplexEn[0] = 0;
    io_cfg.gpioComplexEn[1] = 0;
    stat = CyU3PDeviceConfigureIOMatrix (&io_cfg);
    if (stat != CY_U3P_SUCCESS)
    {
        /* Cannot recover from this error. */
        CyFxAppErrorHandler(stat);
    }

    /* initialize low-speed peripherals */
    UartInit();
    I2cInit();
    GpioInit();

    /* initialize high-speed peripherals */
    GpifInit();
}

void DacModesDeInit()
{
    // TODO
}

void DacModesStart(CyU3PUSBSpeed_t usbSpeed, uint16_t size, CyBool_t is32Wide, const CyU3PGpifConfig_t *cyFxGpifConfig, uint8_t stateIndex, uint8_t initialAlpha)
{
    CyU3PEpConfig_t epCfg;
    CyU3PDmaMultiChannelConfig_t dmaMultiCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyTrue;
    epCfg.epType = CY_U3P_USB_EP_BULK;
    epCfg.burstLen = (usbSpeed == CY_U3P_SUPER_SPEED) ? (CY_FX_EP_BURST_LENGTH) : 1;
    epCfg.streams = 0;
    epCfg.pcktSize = size;

    /* Producer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig (CY_FX_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /*
       Configure the OUT endpoint to allow combining data from multiple buffers into one burst.
       This can help achieve better performance in most cases.
     */
    CyU3PUsbEPSetBurstMode (CY_FX_EP_PRODUCER, CyTrue);

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(CY_FX_EP_PRODUCER);

    CyU3PUsbRegisterEpEvtCallback (CyFxApplnEpCallback, 0x1B0, 0x04, 0x06);

    /* Create a DMA AUTO channel for the USB To GPIF transfer. */
    CyU3PMemSet ((uint8_t *)&dmaMultiCfg, 0, sizeof (dmaMultiCfg));
    dmaMultiCfg.size  = is32Wide ? CY_FX_DMA_BUF_SIZE * 2 :
                                   CY_FX_DMA_BUF_SIZE;
    dmaMultiCfg.count = is32Wide ? CY_FX_DMA_BUF_COUNT / 2 :
                                   CY_FX_DMA_BUF_COUNT;
    dmaMultiCfg.validSckCount = 2;
    dmaMultiCfg.prodSckId[0] = CY_FX_EP_PRODUCER_SOCKET;
    dmaMultiCfg.consSckId[0] = CY_FX_GPIF_CONSUMER_SOCKET_0;
    dmaMultiCfg.consSckId[1] = CY_FX_GPIF_CONSUMER_SOCKET_1;
    dmaMultiCfg.dmaMode = CY_U3P_DMA_MODE_BYTE;
    dmaMultiCfg.prodHeader = 0;
    dmaMultiCfg.prodFooter = 0;
    dmaMultiCfg.consHeader = 0;
    dmaMultiCfg.prodAvailCount = 0;

    // fv
    //dmaMultiCfg.notification = CY_U3P_DMA_CB_CONS_SUSP;
    dmaMultiCfg.notification = CY_U3P_DMA_CB_CONS_SUSP | CY_U3P_DMA_CB_PROD_EVENT;
    dmaMultiCfg.cb = GpifToUsbDmaCallback;
    apiRetStatus = CyU3PDmaMultiChannelCreate (&glDmaChHandle, CY_U3P_DMA_TYPE_AUTO_ONE_TO_MANY, &dmaMultiCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDmaMultiChannelCreate failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set DMA Channel transfer size */
    apiRetStatus = CyU3PDmaMultiChannelSetXfer (&glDmaChHandle, CY_FX_GPIFTOUSB_DMA_TX_SIZE, 0);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDmaMultiChannelSetXfer failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Load and start the GPIF state machine. */
    apiRetStatus = CyU3PGpifLoad (cyFxGpifConfig);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpifLoad failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    apiRetStatus = CyU3PGpifSMStart (stateIndex, initialAlpha);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpifSMStart failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

/* This function stops this DFC mode. This shall be called whenever a RESET
 * or DISCONNECT event is received from the USB host. The endpoints are
 * disabled and the DMA pipe is destroyed by this function. */
void DacModesStop()
{
    CyU3PEpConfig_t epCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* fv - SDDC_FX3 GPIF II state machine checks !FW_TRG to stop */
    CyU3PGpifControlSWInput(CyFalse);

    CyU3PGpifDisable (CyTrue);

    /* Destroy the channels */
    CyU3PDmaMultiChannelDestroy (&glDmaChHandle);

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(CY_FX_EP_PRODUCER);

    /* Disable endpoints. */
    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;

    /* Disable the GPIF->USB endpoint. */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

void DacModesReset()
{
    CyU3PUsbSetEpNak (CY_FX_EP_PRODUCER, CyTrue);
    CyU3PBusyWait (125);

    CyU3PDmaMultiChannelReset (&glDmaChHandle);
    CyU3PUsbFlushEp(CY_FX_EP_PRODUCER);
    CyU3PUsbResetEp (CY_FX_EP_PRODUCER);
    CyU3PDmaMultiChannelSetXfer (&glDmaChHandle, CY_FX_GPIFTOUSB_DMA_TX_SIZE, 0);
    CyU3PUsbStall (CY_FX_EP_PRODUCER, CyFalse, CyTrue);

    CyU3PUsbSetEpNak (CY_FX_EP_PRODUCER, CyFalse);
}

/* this function gets called in the main loop very 10ms or so */
void DacModesLoopActions()
{
    static unsigned long loopCount = 0;

    // show glDMAProdEvent every second or so
    if (++loopCount % 100 == 0) {
        if (glDMAProdEvent != glDMAProdEventPrevIter) {
            CyU3PDebugPrint (4, "glDMAProdEvent: %u (%u)\r\n", glDMAProdEvent, loopCount);
        }
        glDMAProdEventPrevIter = glDMAProdEvent;
    }
}


/* internal functions */

static volatile uint32_t BulkEpEvtCount = 0;    /* Number of endpoint events received on streaming endpoint. */
static uint32_t BulkRstCnt = 0;

/* Endpoint specific event callback. For now, we only keep a count of the endpoint events that occur. */
static void
CyFxApplnEpCallback (
        CyU3PUsbEpEvtType evtype,
        CyU3PUSBSpeed_t   usbSpeed,
        uint8_t           epNum)
{
    CyU3PDebugPrint (2, "EP Event: ep=%x event=%d\r\n", epNum, evtype);
    if (epNum == CY_FX_EP_PRODUCER)
        BulkEpEvtCount++;

    if (evtype == CYU3P_USBEP_SS_RESET_EVT)
    {
        if (epNum == CY_FX_EP_PRODUCER)
        {
            CyU3PDebugPrint (2, "Halting USB Streaming EP: %d\r\n", BulkRstCnt++);
            CyU3PUsbStall (CY_FX_EP_PRODUCER, CyTrue, CyFalse);
        }
    }
}

static void
GpifToUsbDmaCallback (
        CyU3PDmaMultiChannel   *chHandle, 
        CyU3PDmaCbType_t        type,
        CyU3PDmaCBInput_t      *input)
{
    if (type == CY_U3P_DMA_CB_PROD_EVENT) {
        glDMAProdEvent++;
    }
}

/*[]*/
