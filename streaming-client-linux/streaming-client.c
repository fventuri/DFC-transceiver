//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "dfc.h"
#include "stream.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

volatile bool stop_transfers = false;  /* request to stop data transfers */

static void sig_stop(int signum);


int main(int argc, char *argv[])
{
    char *firmware_file = NULL;
    int dfc_mode = 1;
    double samplerate = 32e6;
    double reference_clock = 27e6;
    double reference_ppm = 0;
    int control_interface = 0;
    int data_interface = 0;
    int data_interface_altsetting = 0;
    int endpoint = 0;
    bool cypress_example = false;
    unsigned int reqsize = 16;
    unsigned int queuedepth = 16;
    unsigned int duration = 100;  /* duration of the test in seconds */
    bool show_histogram = false;
    int write_fileno = -1;

    int opt;
    while ((opt = getopt(argc, argv, "f:m:s:x:c:i:e:r:q:t:o:CH")) != -1) {
        switch (opt) {
        case 'f':
            firmware_file = optarg;
            break;
        case 'm':
            if (sscanf(optarg, "%d", &dfc_mode) != 1) {
                fprintf(stderr, "invalid DFC mode: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 's':
            if (sscanf(optarg, "%lf", &samplerate) != 1) {
                fprintf(stderr, "invalid sample rate: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'x':
            if (sscanf(optarg, "%lf", &reference_clock) != 1) {
                fprintf(stderr, "invalid reference clock: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'c':
            if (sscanf(optarg, "%lf", &reference_ppm) != 1) {
                fprintf(stderr, "invalid reference clock correction (ppm): %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'i':
            if (sscanf(optarg, "%d@%d", &data_interface, &data_interface_altsetting) != 2) {
                if (sscanf(optarg, "%d", &data_interface) != 1) {
                    fprintf(stderr, "invalid data interface number: %s\n", optarg);
                    return EXIT_FAILURE;
                }
            }
            break;
        case 'e':
            if (sscanf(optarg, "%d", &endpoint) != 1) {
                fprintf(stderr, "invalid endpoint: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'r':
            if (sscanf(optarg, "%u", &reqsize) != 1) {
                fprintf(stderr, "invalid request size: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'q':
            if (sscanf(optarg, "%u", &queuedepth) != 1) {
                fprintf(stderr, "invalid queue depth: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 't':
            if (sscanf(optarg, "%u", &duration) != 1) {
                fprintf(stderr, "invalid duration: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'o':
            if (strcmp(optarg, "-") == 0) {
                write_fileno = STDOUT_FILENO;
            } else {
                write_fileno = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (write_fileno == -1) {
                    fprintf(stderr, "open(%s) for writing failed: %s\n", optarg, strerror(errno));
                    return EXIT_FAILURE;
                }
            }
            break;
        case 'C':
            cypress_example = true;
            break;
        case 'H':
            show_histogram = true;
            break;
        case '?':
            /* invalid option */
            return EXIT_FAILURE;
        }
    }

    if (firmware_file == NULL) {
        fprintf(stderr, "missing firmware file\n");
        return EXIT_FAILURE;
    }

    if (show_histogram && write_fileno == STDOUT_FILENO) {
        fprintf(stderr, "[ERROR] options -H (show histogram) and -o - (write to stdout) are mutually exclusive\n");
        return EXIT_FAILURE;
    }

    dfc_t dfc;
    int status;

    status = usb_init(&dfc.usb_device, firmware_file);
    if (status == -1) {
        return EXIT_FAILURE;
    }

    status = usb_open(&dfc.usb_device, control_interface, data_interface, data_interface_altsetting, endpoint);
    if (status == -1) {
        return EXIT_FAILURE;
    }

    if (!cypress_example) {
        fprintf(stderr, "DFC FW version: %s\n", dfc_fx3_get_fw_version(&dfc));

        if (dfc_mode > 0) {
            const uint8_t SETMODE = 0x90;
            uint8_t data = dfc_mode;
            status = usb_control_write(&dfc.usb_device, SETMODE, &data, sizeof(data));
            if (status != 0) {
                fprintf(stderr, "set DFC mode to %d failed\n", dfc_mode);
                return -1;
            }
        }

        /* wait a few ms before using the new mode */
        usleep(20000);

        fprintf(stderr, "DFC mode: %hhu\n", dfc_fx3_get_mode(&dfc));

        status = clock_start(&dfc.clock, &dfc.usb_device, reference_clock * (1.0 + 1e-6 * reference_ppm), samplerate);
        if (status == -1) {
            return EXIT_FAILURE;
        }

        status = dfc_fx3_start(&dfc);
        if (status == -1) {
            usb_close(&dfc.usb_device);
            return EXIT_FAILURE;
        }
    }

    if (duration > 0) {
        stream_t stream;

        status = stream_init(&stream, &dfc.usb_device, reqsize, queuedepth, show_histogram, write_fileno);
        if (status == -1) {
            usb_close(&dfc.usb_device);
            return EXIT_FAILURE;
        }

        struct sigaction sigact;

        sigact.sa_handler = sig_stop;
        sigemptyset(&sigact.sa_mask);
        sigact.sa_flags = 0;
        (void)sigaction(SIGINT, &sigact, NULL);
        (void)sigaction(SIGTERM, &sigact, NULL);
        ( void)sigaction(SIGALRM, &sigact, NULL);

        status = stream_start(&stream);
        if (status == -1) {
            usb_close(&dfc.usb_device);
            return EXIT_FAILURE;
        }

        alarm(duration);

        while (!stop_transfers) {
            libusb_handle_events(NULL);
        }

        status = stream_stop(&stream);
        if (status == -1) {
            usb_close(&dfc.usb_device);
            return EXIT_FAILURE;
        }

        stream_stats(duration);

        status = stream_fini(&stream);
        if (status == -1) {
            usb_close(&dfc.usb_device);
            return EXIT_FAILURE;
        }
    }

    if (!cypress_example) {
        status = dfc_fx3_stop(&dfc);
        if (status == -1) {
            usb_close(&dfc.usb_device);
            return EXIT_FAILURE;
        }
    }

    if (!(write_fileno == -1 || write_fileno == STDOUT_FILENO)) {
        close(write_fileno);
    }

    status = usb_close(&dfc.usb_device);
    if (status == -1) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void sig_stop(int signum) {
    (void)signum;
    fprintf(stderr, "Abort. Stopping transfers\n");
    stop_transfers = true;
}
