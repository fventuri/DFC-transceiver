//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "cyu3system.h"
#include "cyu3error.h"

#include "app.h"
#include "dfc_modes.h"
#include "uart.h"


static void Init() {
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PReturnStatus_t stat;

    CyU3PMemSet ((uint8_t *)&io_cfg, 0, sizeof (io_cfg));
    io_cfg.isDQ32Bit = CyFalse;
    io_cfg.useUart   = CyTrue;
    io_cfg.useI2C    = CyFalse;
    io_cfg.useI2S    = CyFalse;
    io_cfg.useSpi    = CyFalse;
    io_cfg.lppMode   = CY_U3P_IO_MATRIX_LPP_UART_ONLY;
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
}

static void DeInit()
{
    /* deinitialize low-speed peripherals */
    UartDeInit();
}

/* this function gets called in the main loop very 10ms or so */
static void LoopActions()
{
    static unsigned long loopCount = 0;

    // send out a test message every second or so
    if (++loopCount % 100 == 0) {
        CyU3PDebugPrint (4, "loopCount=%u\r\n", loopCount);
    }
}


/* mode definition */
DFCMode UART_ONLY = {
    "UART_ONLY",
    Init,
    DeInit,
    NULL,
    NULL,
    NULL,
    LoopActions
};

/*[]*/
