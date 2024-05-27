//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "dfc_modes.h"

extern DFCMode UART_ONLY;
extern DFCMode SINGLE_ADC;
extern DFCMode DUAL_ADC;
extern DFCMode DAC;
extern DFCMode SINGLE_ADC_FX3_CLOCK;
extern DFCMode DAC_FX3_CLOCK;

DFCMode *DFCModes[] = {
   &UART_ONLY,
   &SINGLE_ADC,
   &DUAL_ADC,
   &DAC,
   &SINGLE_ADC_FX3_CLOCK,
   &DAC_FX3_CLOCK,
};

int NumDFCModes = sizeof(DFCModes) / sizeof(DFCModes[0]);

/*[]*/

