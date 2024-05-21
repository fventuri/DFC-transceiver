//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef _INCLUDED_ADC_MODES_H_
#define _INCLUDED_ADC_MODES_H_

#include "cyu3system.h"
#include "cyu3usb.h"
#include "cyu3gpif.h"

void AdcModesInit(CyBool_t is32Wide);
void AdcModesDeInit();
void AdcModesStart(CyU3PUSBSpeed_t usbSpeed, uint16_t size, CyBool_t is32Wide, const CyU3PGpifConfig_t *cyFxGpifConfig, uint8_t stateIndex, uint8_t initialAlpha);
void AdcModesStop();
void AdcModesReset();
void AdcModesLoopActions();

#endif /* _INCLUDED_ADC_MODES_H_ */

/*[]*/
