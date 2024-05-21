//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "dfc.h"

#include <stdio.h>

int dfc_fx3_start(dfc_t *this)
{
    const uint8_t STARTFX3 = 0xaa;

    int status;
    status = usb_control_write(&this->usb_device, STARTFX3, 0, 0);
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_start() failed\n");
        return -1;
    }

    return 0;
}

int dfc_fx3_stop(dfc_t *this)
{
    const uint8_t STOPFX3 = 0xab;

    int status;
    status = usb_control_write(&this->usb_device, STOPFX3, 0, 0);
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_stop() failed\n");
        return -1;
    }

    return 0;
}
