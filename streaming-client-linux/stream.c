//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "stream.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const unsigned int timeout = 5000;  /* timeout (in ms) for each transfer */

static atomic_int active_transfers;
static bool stop_transfers = false;
/* stream stats */
/* TODO: move them to their own structure inside a stream structure */
static unsigned int success_count = 0;         // number of successful transfers
static unsigned int failure_count = 0;         // number of failed transfers
static unsigned long long transfer_size = 0;   // total size of data transfers
static short sample_even_min = SHRT_MAX;       // minimum even sample value
static short sample_even_max = SHRT_MIN;       // maximum even sample value
static short sample_odd_min = SHRT_MAX;        // minimum odd sample value
static short sample_odd_max = SHRT_MIN;        // maximum odd sample value

static const int SIXTEEN_BITS_SIZE = 65536;
static unsigned long long *histogram_even = NULL;   // histogram for even samples
static unsigned long long *histogram_odd = NULL;    // histogram for odd samples

static uint8_t *read_buffer = NULL;
static unsigned long long input_data_size = 0;      // total size of input data


static void LIBUSB_CALL transfer_callback(struct libusb_transfer *transfer) ;


int stream_init(stream_t *this, stream_direction_t direction, int read_write_fileno, usb_device_t *usb_device, int num_packets_per_transfer, int num_concurrent_transfers, bool show_histogram)
{
    this->usb_device = usb_device;
    this->direction = direction;
    this->read_write_fileno = read_write_fileno;
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

    /* allocate read buffer if direction is STREAM_TX */
    if (direction == STREAM_TX) {
        read_buffer = (uint8_t *)malloc(this->transfer_size / 2);
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

    if (show_histogram) {
        histogram_even = (unsigned long long *) malloc(SIXTEEN_BITS_SIZE * sizeof(unsigned long long));
        histogram_odd = (unsigned long long *) malloc(SIXTEEN_BITS_SIZE * sizeof(unsigned long long));
        for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
            histogram_even[i] = 0;
        }   
        for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
            histogram_odd[i] = 0;
        }
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

    if (histogram_even != NULL) {
        free(histogram_even);
    }
    if (histogram_odd != NULL) {
        free(histogram_odd);
    }

    if (read_buffer != NULL) {
        free(read_buffer);
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

void stream_stats(stream_t *this, unsigned int duration)
{
    fprintf(stderr, "success count: %u\n", success_count);
    fprintf(stderr, "failure count: %u\n", failure_count);
    fprintf(stderr, "transfer size: %llu B\n", transfer_size);
    fprintf(stderr, "transfer rate: %.0lf kB/s\n", (double) transfer_size / duration / 1024.0);
    if (this->direction == STREAM_RX) {
        fprintf(stderr, "even samples range: [%hd,%hd]\n", sample_even_min, sample_even_max);
        fprintf(stderr, "odd samples range: [%hd,%hd]\n", sample_odd_min, sample_odd_max);

        if (histogram_even != NULL) {
            int histogram_min = -1;
            int histogram_max = -1;
            unsigned long long total_histogram_samples = 0;
            for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
                if (histogram_even[i] > 0) {
                    if (histogram_min < 0) {
                        histogram_min = i;
                    }
                    histogram_max = i;
                    total_histogram_samples += histogram_even[i];
                }
            }
            if (total_histogram_samples > 0) {
                fprintf(stdout, "# Even samples histogram\n");
                for (int i = histogram_min; i <= histogram_max; i++) {
                    fprintf(stdout, "%d\t%llu\n", i - SIXTEEN_BITS_SIZE / 2,
                            histogram_even[i]);
                }
                fprintf(stdout, "\n");
            }
            fprintf(stderr, "total even histogram samples: %llu\n", total_histogram_samples);
        }

        if (histogram_odd != NULL) {
            int histogram_min = -1;
            int histogram_max = -1;
            unsigned long long total_histogram_samples = 0;
            for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
                if (histogram_odd[i] > 0) {
                    if (histogram_min < 0) {
                        histogram_min = i;
                    }
                    histogram_max = i;
                    total_histogram_samples += histogram_odd[i];
                }
            }
            if (total_histogram_samples > 0) {
                fprintf(stdout, "# Odd samples histogram\n");
                for (int i = histogram_min; i <= histogram_max; i++) {
                    fprintf(stdout, "%d\t%llu\n", i - SIXTEEN_BITS_SIZE / 2,
                            histogram_odd[i]);
                }
                fprintf(stdout, "\n");
            }
            fprintf(stderr, "total odd histogram samples: %llu\n", total_histogram_samples);
        }
    } else if (this->direction == STREAM_TX) {
        fprintf(stderr, "input data size: %llu B\n", input_data_size);
    }

    return;
}


/* internal functions */
static int stream_rx_callback(stream_t *this, uint8_t *buffer, int length);
static int stream_tx_callback(stream_t *this, uint8_t *buffer, int length);

static void LIBUSB_CALL transfer_callback(struct libusb_transfer *transfer) 
{
    stream_t *this __attribute__ ((unused)) = (stream_t *) transfer->user_data;
    atomic_fetch_sub(&active_transfers, 1);
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        /* success!!! */
        success_count++;
        stream_t *stream = (stream_t *)transfer->user_data;
        switch (stream->direction) {
        case STREAM_RX:
            if (stream_rx_callback(stream, transfer->buffer, transfer->actual_length) == -1) {
                stop_transfers = true;
            }
            break;
        case STREAM_TX:
            if (stream_tx_callback(stream, transfer->buffer, transfer->actual_length) == -1) {
                stop_transfers = true;
            }
            break;
        }
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

static int stream_rx_callback(stream_t *this, uint8_t *buffer, int length)
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

    if (histogram_even != NULL) {
        for (int i = 0; i < nsamples; i += 2) {
            histogram_even[samples[i] + SIXTEEN_BITS_SIZE / 2]++;
        }
    }
    if (histogram_odd != NULL) {
        for (int i = 1; i < nsamples; i += 2) {
            histogram_odd[samples[i] + SIXTEEN_BITS_SIZE / 2]++;
        }
    }

    if (this->read_write_fileno >= 0) {
        size_t remaining = length;
        while (remaining > 0) {
            ssize_t written = write(this->read_write_fileno, buffer + (length - remaining), remaining);
            if (written == -1) {
                fprintf(stderr, "write to output file failed - error: %s\n", strerror(errno));
                /* if there's any error stop writing to output file */
                this->read_write_fileno = -1;
                break;
            } else {
                remaining -= written;
            }
        }
    }

#ifdef _BUFFER_INDEX_CHECK_
    /* buffer index check */
    static int buffer_index = 0;
    if (buffer_index >= 0) {
        if (this->buffers[buffer_index] == buffer) {
            buffer_index = (buffer_index + 1) % this->num_concurrent_transfers;
        } else {
            bool found = false;
            for (int i = 0; i < this->num_concurrent_transfers; i++) {
                if (this->buffers[i] == buffer) {
                    fprintf(stderr, "warning - skipped at least %d buffers\n",
                            (this->num_concurrent_transfers + i - buffer_index) % this->num_concurrent_transfers);
                    buffer_index = (i + 1) % this->num_concurrent_transfers;
                    found = true;
                    break;
                }
            }
            if (!found) {
                fprintf(stderr, "error - cannot find buffer index for %p\n", buffer);
                /* stop checking at this point */
                buffer_index = -1;
            }
        }
    }
#endif  /* _BUFFER_INDEX_CHECK_ */

    return 0;
}

static int stream_tx_callback(stream_t *this, uint8_t *buffer, int length)
{
    short *samples = (short *)buffer;
    int nsamples __attribute__((unused)) = length / sizeof(samples[0]);

    /* read nsamples/2 from input because of interleaving them with 0's - see below */
    size_t remaining = length / 2;
    while (remaining > 0) {
        ssize_t nread = read(this->read_write_fileno, read_buffer + (length / 2 - remaining), remaining);
        if (nread == -1) {
            fprintf(stderr, "read from input file/stdin failed - error: %s\n", strerror(errno));
            /* if there's any error stop reading from input file */
            this->read_write_fileno = -1;
            return -1;
            break;
        } else if (nread == 0) {
            /* EOF - send a message and exit */
            fprintf(stderr, "EOF from input file/stdin. Done streaming\n");
            return -1;
            break;
        } else {
            remaining -= nread;
        }
    }

    /* interleave the 16 bit samples with all 0 since because we are running 32 bit wide */
    /* shift the values by 2 bits because the DAC is comnnected to bits 2:15 */
    short *insamples = (short *)read_buffer;
    int ninsamples = (length / 2 - remaining) / sizeof(insamples[0]);
    for (int i = 0, j = 0; i < ninsamples; i++, j += 2) {
        samples[j] = 0;
        samples[j+1] = insamples[i] << 2;
    }

    transfer_size += length;
    input_data_size += ninsamples * sizeof(insamples[0]);

    return 0;
}
