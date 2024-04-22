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

    /* no data, but we need to pass something otherwise lib_control_transfer hangs */
    uint32_t data = 0;
    int status;
    status = usb_control_write(&this->usb_device, STARTFX3, (uint8_t *)&data, sizeof(data));
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_start() failed\n");
        return -1;
    }

    return 0;
}

int dfc_fx3_stop(dfc_t *this)
{
    const uint8_t STOPFX3 = 0xab;

    /* no data, but we need to pass something otherwise lib_control_transfer hangs */
    uint32_t data = 0;
    int status;
    status = usb_control_write(&this->usb_device, STOPFX3, (uint8_t *)&data, sizeof(data));
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_stop() failed\n");
        return -1;
    }

    return 0;
}
