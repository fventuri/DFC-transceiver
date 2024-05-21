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

DFCMode *DFCModes[] = {
   &UART_ONLY,
   &SINGLE_ADC,
   &DUAL_ADC,
};

int NumDFCModes = sizeof(DFCModes) / sizeof(DFCModes[0]);

/*[]*/

