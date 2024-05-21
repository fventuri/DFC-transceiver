//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef _INCLUDED_DFC_MODES_H_
#define _INCLUDED_DFC_MODES_H_

#include "cyu3usb.h"

typedef struct {
    const char *name;
    void (*Init)();
    void (*DeInit)();
    void (*Start)(CyU3PUSBSpeed_t usbSpeed, uint16_t size);
    void (*Stop)();
    void (*Reset)();
    void (*LoopActions)();
} DFCMode;

extern DFCMode *DFCModes[];
extern int NumDFCModes;

#endif /* _INCLUDED_DFC_MODES_H_ */

/*[]*/
