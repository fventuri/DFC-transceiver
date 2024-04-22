//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "klock.h"

#include <stdio.h>

int clock_start(klock_t *this, usb_device_t *usb_device, double samplerate)
{
    const uint8_t STARTADC = 0xb2;

    uint32_t samplerate_int = (uint32_t)samplerate;

    int status;
    status = usb_control_write(usb_device, STARTADC, (uint8_t *)&samplerate_int, sizeof(samplerate_int));
    if (status != 0) {
        fprintf(stderr, "clock_start(%d) failed\n", samplerate_int);
        return -1;
    }

    return 0;
}
