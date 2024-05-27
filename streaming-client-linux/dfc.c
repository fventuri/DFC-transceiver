//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "dfc.h"

#include <stdio.h>

const uint8_t* dfc_fx3_get_fw_version(dfc_t *this)
{
    static uint8_t fw_version[64];
    const uint8_t GETFWVERSION = 0x01;

    int status;
    status = usb_control_read(&this->usb_device, GETFWVERSION, fw_version, sizeof(fw_version));
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_get_fw_version() failed\n");
        return NULL;
    }

    return fw_version;
}

uint8_t dfc_fx3_get_mode(dfc_t *this)
{
    uint8_t dfc_mode;
    const uint8_t GETMODE = 0x10;

    int status;
    status = usb_control_read(&this->usb_device, GETMODE, &dfc_mode, sizeof(dfc_mode));
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_get_mode() failed\n");
        return (uint8_t)-1;
    }

    return dfc_mode;
}

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

int dfc_fx3_shutdown_adc(dfc_t *this)
{
    const uint8_t SHUTDOWNADC = 0xc1;

    int status;
    status = usb_control_write(&this->usb_device, SHUTDOWNADC, 0, 0);
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_shutdown_adc() failed\n");
        return -1;
    }

    return 0;
}

int dfc_fx3_wakeup_adc(dfc_t *this)
{
    const uint8_t WAKEUPADC = 0xc2;

    int status;
    status = usb_control_write(&this->usb_device, WAKEUPADC, 0, 0);
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_wakeup_adc() failed\n");
        return -1;
    }

    return 0;
}

int dfc_fx3_shutdown_dac(dfc_t *this)
{
    const uint8_t SHUTDOWNDAC = 0xc3;

    int status;
    status = usb_control_write(&this->usb_device, SHUTDOWNDAC, 0, 0);
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_shutdown_dac() failed\n");
        return -1;
    }

    return 0;
}

int dfc_fx3_wakeup_dac(dfc_t *this)
{
    const uint8_t WAKEUPDAC = 0xc4;

    int status;
    status = usb_control_write(&this->usb_device, WAKEUPDAC, 0, 0);
    if (status != 0) {
        fprintf(stderr, "dfc_fx3_wakeup_dac() failed\n");
        return -1;
    }

    return 0;
}
