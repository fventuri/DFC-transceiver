//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "stream.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const unsigned int timeout = 5000;  /* timeout (in ms) for each transfer */

static atomic_int active_transfers;
static bool stop_transfers = false;
/* stream stats */
static unsigned int success_count = 0;         // number of successful transfers
static unsigned int failure_count = 0;         // number of failed transfers
static unsigned long long transfer_size = 0;   // total size of data transfers
static short sample_even_min = SHRT_MAX;       // minimum even sample value
static short sample_even_max = SHRT_MIN;       // maximum even sample value
static short sample_odd_min = SHRT_MAX;        // minimum odd sample value
static short sample_odd_max = SHRT_MIN;        // maximum odd sample value

static void LIBUSB_CALL transfer_callback(struct libusb_transfer *transfer) ;


int stream_init(stream_t *this, usb_device_t *usb_device, int num_packets_per_transfer, int num_concurrent_transfers)
{
    this->usb_device = usb_device;
    this->num_packets_per_transfer = num_packets_per_transfer;
    this->num_concurrent_transfers = num_concurrent_transfers;
    this->transfer_size = num_packets_per_transfer * usb_device->packet_size;

    /* allocate transfer buffers for zerocopy USB bulk transfers */
    this->buffers = (uint8_t **)malloc(num_concurrent_transfers * sizeof(uint8_t *));
    for (int i = 0; i < num_concurrent_transfers; i++) {
        this->buffers[i] = libusb_dev_mem_alloc(usb_device->device_handle, this->transfer_size);
        if (this->buffers[i] == NULL) {
            fprintf(stderr, "stream_init - libusb_dev_mem_alloc() failed\n");
            for (int j = i - 1; j >= 0; j--) {
                libusb_dev_mem_free(usb_device->device_handle, this->buffers[j], this->transfer_size);
            }
            return -1;
        }
    }

    /* populate the required libusb_transfer fields */
    this->transfers = (struct libusb_transfer **)malloc(num_concurrent_transfers * sizeof(struct libusb_transfer *));
    for (int i = 0; i < num_concurrent_transfers; i++) {
        this->transfers[i] = libusb_alloc_transfer(0);  /* bulk transfers */
        libusb_fill_bulk_transfer(this->transfers[i],
                                  usb_device->device_handle,
                                  usb_device->bEndpointAddress,
                                  this->buffers[i],
                                  this->transfer_size,
                                  transfer_callback,
                                  this,
                                  timeout);
    }

    return 0;
}

int stream_fini(stream_t *this)
{
    if (this->transfers) {
        for (int i = this->num_concurrent_transfers - 1; i >= 0; i--) {
            libusb_free_transfer(this->transfers[i]);
        }
        free(this->transfers);
    }

    if (this->buffers) {
        for (int i = this->num_concurrent_transfers - 1; i >= 0; i--) {
            libusb_dev_mem_free(this->usb_device->device_handle, this->buffers[i], this->transfer_size);
        }
        free(this->buffers);
    }

    return 0;
}

int stream_start(stream_t *this)
{
    /* submit all the transfers */
    stop_transfers = false;
    atomic_init(&active_transfers, 0);
    for (int i = 0; i < this->num_concurrent_transfers; i++) {
        int status = libusb_submit_transfer(this->transfers[i]);
        if (status != LIBUSB_SUCCESS) {
            fprintf(stderr, "stream_start - error in libusb_submit_transfer(): %s\n", libusb_strerror(status));
            return -1;
        }
        atomic_fetch_add(&active_transfers, 1);
    }

    return 0;
}

int stream_stop(stream_t *this)
{
    stop_transfers = true;
    /* cancel all the active transfers */
    bool ok = true;
    for (int i = 0; i < this->num_concurrent_transfers; i++) {
        int status = libusb_cancel_transfer(this->transfers[i]);
        if (status != LIBUSB_SUCCESS) {
            if (status == LIBUSB_ERROR_NOT_FOUND) {
                continue;
            }
            fprintf(stderr, "stream_stop - error in libusb_cancel_transfer(): %s\n", libusb_strerror(status));
            ok = false;
          }
    }

#if 0
    /* flush all the events */
    struct timeval noblock = { 0, 0 };
    int status = libusb_handle_events_timeout_completed(NULL, &noblock, 0);
    if (status == LIBUSB_ERROR_NOT_FOUND) {
        fprintf(stderr, "stream_stop - error in libusb_cancel_transfer(): %s\n", libusb_strerror(status));
        ok = false;
    }
#endif
    while (active_transfers > 0) {
        libusb_handle_events(NULL);
        usleep(100);
    }

    return ok ? 0 : - 1;
}

void stream_stats(unsigned int duration)
{
    fprintf(stderr, "success count: %u\n", success_count);
    fprintf(stderr, "failure count: %u\n", failure_count);
    fprintf(stderr, "transfer size: %llu B\n", transfer_size);
    fprintf(stderr, "transfer rate: %.0lf kB/s\n", (double) transfer_size / duration / 1024.0);
    fprintf(stderr, "even samples range: [%hd,%hd]\n", sample_even_min, sample_even_max);
    fprintf(stderr, "odd samples range: [%hd,%hd]\n", sample_odd_min, sample_odd_max);
}


/* internal functions */
static void stream_callback(uint8_t *buffer, int length);

static void LIBUSB_CALL transfer_callback(struct libusb_transfer *transfer) 
{
    stream_t *this __attribute__ ((unused)) = (stream_t *) transfer->user_data;
    atomic_fetch_sub(&active_transfers, 1);
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        /* success!!! */
        success_count++;
        stream_callback(transfer->buffer, transfer->actual_length);
        if (!stop_transfers) {
            int status = libusb_submit_transfer(transfer);
            if (status != LIBUSB_SUCCESS) {
                fprintf(stderr, "transfer_callback - error in libusb_submit_transfer(): %s\n", libusb_strerror(status));
                return;
            }
            atomic_fetch_add(&active_transfers, 1);
        }
    } else if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
        /* ignore LIBUSB_TRANSFER_CANCELLED */
        return;
    } else {
        failure_count++;
        fprintf(stderr, "transfer_callback - error in transfer->status: %s\n", libusb_error_name(transfer->status));

#if 0
        /* cancel all the active transfers */
        for (int i = 0; i < this->num_concurrent_transfers; i++) {
            int status = libusb_cancel_transfer(this->transfers[i]);
            if (status != LIBUSB_SUCCESS) {
                if (status == LIBUSB_ERROR_NOT_FOUND) {
                    continue;
                }
                fprintf(stderr, "transfer_callback - error in libusb_cancel_transfer(): %s\n", libusb_strerror(status));
            }
        }
#endif
    }

    return;
}

static void stream_callback(uint8_t *buffer, int length)
{
    transfer_size += length;
    short *samples = (short *)buffer;
    int nsamples = length / sizeof(samples[0]);
    for (int i = 0; i < nsamples; i++) {
        if (i % 2 == 0) {
            sample_even_min = samples[i] < sample_even_min ? samples[i] : sample_even_min;
            sample_even_max = samples[i] > sample_even_max ? samples[i] : sample_even_max;
        } else {
            sample_odd_min = samples[i] < sample_odd_min ? samples[i] : sample_odd_min;
            sample_odd_max = samples[i] > sample_odd_max ? samples[i] : sample_odd_max;
        }
    }
    return;
}
