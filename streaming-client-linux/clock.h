//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef _STREAMING_CLIENT_CLOCK_H_
#define _STREAMING_CLIENT_CLOCK_H_

#include "usb.h"

typedef struct {
    double samplerate;
    double reference;
} dfcclock_t;

int clock_start(dfcclock_t *this, usb_device_t *usb_device, double reference, double samplerate);

#endif /* _STREAMING_CLIENT_CLOCK_H_ */
