//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "dac_modes.h"
#include "dfc_modes.h"
#include "cyu3system.h"
#include "cyfxgpif2config-dac-fx3-clock.h"

static void Init() {
    DacModesInit(CyTrue);
}

static void DeInit()
{
    DacModesDeInit();
}

static void Start(CyU3PUSBSpeed_t usbSpeed, uint16_t size)
{
    DacModesStart(usbSpeed, size, CyTrue, &CyFxGpifConfig, START, ALPHA_START);
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
DFCMode DAC_FX3_CLOCK = {
    "DAC_FX3_CLOCK",
    Init,
    DeInit,
    Start,
    Stop,
    Reset,
    LoopActions
};

/*[]*/
