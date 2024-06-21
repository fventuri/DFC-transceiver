//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "dac_modes.h"
#include "dfc_modes.h"
#include "cyu3system.h"
#include "cyfxgpif2config-dac.h"

static void Init() {
    DacModesInit(CyFalse);
}

static void DeInit()
{
    DacModesDeInit();
}

static void Start(CyU3PUSBSpeed_t usbSpeed, uint16_t size)
{
    DacModesStart(usbSpeed, size, CyFalse, &CyFxGpifConfig, START, ALPHA_START);
}

static void Stop()
{
    DacModesStop();
}

static void Reset()
{
    DacModesReset();
}

static void LoopActions()
{
    DacModesLoopActions();
}


/* mode definition */
DFCMode DAC = {
    "DAC",
    Init,
    DeInit,
    Start,
    Stop,
    Reset,
    LoopActions
};

/*[]*/
