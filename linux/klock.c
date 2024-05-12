//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "klock.h"

#include <stdio.h>

int clock_start(__attribute__ ((unused)) klock_t *this, usb_device_t *usb_device, double reference, double samplerate)
{
    const uint8_t STARTADC = 0xb2;

    double data[] = { reference, samplerate };

    int status;
    status = usb_control_write(usb_device, STARTADC, (uint8_t *)data, sizeof(data));
    if (status != 0) {
        fprintf(stderr, "clock_start(%lf, %lf) failed\n", reference, samplerate);
        return -1;
    }

    return 0;
}
