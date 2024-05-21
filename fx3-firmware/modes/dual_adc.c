//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "adc_modes.h"
#include "dfc_modes.h"
#include "cyu3system.h"
#include "cyfxgpif2config-dual-adc.h"

static void Init() {
    AdcModesInit(CyTrue);
}

static void DeInit()
{
    AdcModesDeInit();
}

static void Start(CyU3PUSBSpeed_t usbSpeed, uint16_t size)
{
    AdcModesStart(usbSpeed, size, CyTrue, &CyFxGpifConfig, START, ALPHA_START);
}

static void Stop()
{
    AdcModesStop();
}

static void Reset()
{
    AdcModesReset();
}

static void LoopActions()
{
    AdcModesLoopActions();
}


/* mode definition */
DFCMode DUAL_ADC = {
    "DUAL_ADC",
    Init,
    DeInit,
    Start,
    Stop,
    Reset,
    LoopActions
};

/*[]*/
