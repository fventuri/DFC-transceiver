//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef _STREAMING_CLIENT_DFC_H_
#define _STREAMING_CLIENT_DFC_H_

#include "usb.h"
#include "clock.h"

typedef struct {
    usb_device_t usb_device;
    dfcclock_t clock;
} dfc_t;

int dfc_fx3_start(dfc_t *this);
int dfc_fx3_stop(dfc_t *this);

#endif /* _STREAMING_CLIENT_DFC_H_ */
