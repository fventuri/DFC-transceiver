//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef _STREAMING_CLIENT_STREAM_H_
#define _STREAMING_CLIENT_STREAM_H_

#include "usb.h"

typedef struct {
    usb_device_t *usb_device;
    int num_packets_per_transfer;
    int num_concurrent_transfers;
    int transfer_size;
    uint8_t **buffers;
    struct libusb_transfer **transfers;
} stream_t;

int stream_init(stream_t *this, usb_device_t *usb_device, int num_packets_per_transfer, int num_concurrent_transfers);
int stream_fini(stream_t *this);
int stream_start(stream_t *this);
int stream_stop(stream_t *this);
void stream_stats();

#endif /* _STREAMING_CLIENT_STREAM_H_ */
