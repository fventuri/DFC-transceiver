//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef _STREAMING_CLIENT_KLOCK_H_
#define _STREAMING_CLIENT_KLOCK_H_

#include "usb.h"

typedef struct {
    double samplerate;
    double reference;
    double correction_ppm;
} klock_t;

int clock_start(klock_t *this, usb_device_t *usb_device, double samplerate);

#endif /* _STREAMING_CLIENT_KLOCK_H_ */
