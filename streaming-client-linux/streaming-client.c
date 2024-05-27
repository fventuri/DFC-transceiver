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

typedef enum {
    UART_ONLY,
    SINGLE_ADC,
    DUAL_ADC,
    DAC,
    SINGLE_ADC_FX3_CLOCK,
    DAC_FX3_CLOCK,
    DFC_MODE_UNKNOWN = -1
} dfc_mode_t;

volatile bool stop_transfers = false;  /* request to stop data transfers */

static void sig_stop(int signum);


int main(int argc, char *argv[])
{
    char *firmware_file = NULL;
    dfc_mode_t dfc_mode = DFC_MODE_UNKNOWN;
    double samplerate = 32e6;
    double reference_clock = 27e6;
    double reference_ppm = 0;
    int control_interface = 0;
    int data_interface = 0;
    int data_interface_altsetting = 0;
    int endpoint = -1;
    bool cypress_example = false;
    unsigned int reqsize = 16;
    unsigned int queuedepth = 16;
    unsigned int duration = 100;  /* duration of the test in seconds */
    bool show_histogram = false;
    int write_fileno = -1;
    int read_fileno = -1;

    int opt;
    while ((opt = getopt(argc, argv, "f:m:s:x:c:j:e:r:q:t:o:i:CH")) != -1) {
        switch (opt) {
        case 'f':
            firmware_file = optarg;
            break;
        case 'm':
            if (sscanf(optarg, "%d", (int *)&dfc_mode) != 1) {
                if (strcmp(optarg, "UART-ONLY") == 0) {
                    dfc_mode = UART_ONLY;
                } else if (strcmp(optarg, "SINGLE-ADC") == 0) {
                    dfc_mode = SINGLE_ADC;
                } else if (strcmp(optarg, "DUAL-ADC") == 0) {
                    dfc_mode = DUAL_ADC;
                } else if (strcmp(optarg, "DAC") == 0) {
                    dfc_mode = DAC;
                } else if (strcmp(optarg, "SINGLE-ADC-FX3-CLOCK") == 0) {
                    dfc_mode = SINGLE_ADC_FX3_CLOCK;
                } else if (strcmp(optarg, "DAC-FX3-CLOCK") == 0) {
                    dfc_mode = DAC_FX3_CLOCK;
                } else {
                    fprintf(stderr, "invalid DFC mode: %s\n", optarg);
                    return EXIT_FAILURE;
                }
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
        case 'j':
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
        case 'i':
            if (strcmp(optarg, "-") == 0) {
                read_fileno = STDIN_FILENO;
            } else {
                read_fileno = open(optarg, O_RDONLY);
                if (read_fileno == -1) {
                    fprintf(stderr, "open(%s) for reading failed: %s\n", optarg, strerror(errno));
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

    if (read_fileno >= 0 && (write_fileno >= 0 || show_histogram)) {
        fprintf(stderr, "[ERROR] options -i (read from stdin/file) and -o (write to stdout/file) or -H (show histogram) are exclusive\n");
        fprintf(stderr, "[ERROR] streaming-client cannot not write and read at the same time (no full-duplex yet)\n");
        if (read_fileno != STDIN_FILENO) {
            close(read_fileno);
        }
        if (write_fileno != STDOUT_FILENO) {
            close(write_fileno);
        }
        return EXIT_FAILURE;
    }

    if (show_histogram && write_fileno == STDOUT_FILENO) {
        fprintf(stderr, "[ERROR] options -H (show histogram) and -o - (write to stdout) are mutually exclusive\n");
        return EXIT_FAILURE;
    }

    if (firmware_file == NULL) {
        fprintf(stderr, "missing firmware file\n");
        return EXIT_FAILURE;
    }

    stream_direction_t stream_direction = read_fileno >= 0 ? STREAM_TX : STREAM_RX;
    int stream_read_write_fileno = read_fileno >= 0 ? read_fileno : write_fileno;
    if (dfc_mode == DFC_MODE_UNKNOWN) {
        switch (stream_direction) {
        case STREAM_RX:
            dfc_mode = SINGLE_ADC;
            break;
        case STREAM_TX:
            dfc_mode = DAC;
            break;
        }
    }

    if (stream_direction == STREAM_RX) {
        if (!(dfc_mode == SINGLE_ADC || dfc_mode == DUAL_ADC || dfc_mode == SINGLE_ADC_FX3_CLOCK)) {
            fprintf(stderr, "invalid DFC mode for RX stream direction\n");
            return EXIT_FAILURE;
        }
    } else if (stream_direction == STREAM_TX) {
        if (!(dfc_mode == DAC || dfc_mode == DAC_FX3_CLOCK)) {
            fprintf(stderr, "invalid DFC mode for TX stream direction\n");
            return EXIT_FAILURE;
        }
    }

    dfc_t dfc;
    int status;

    status = usb_init(&dfc.usb_device, firmware_file);
    if (status == -1) {
        return EXIT_FAILURE;
    }

    status = usb_open(&dfc.usb_device, control_interface, data_interface, data_interface_altsetting, endpoint, stream_direction);
    if (status == -1) {
        return EXIT_FAILURE;
    }

    if (!cypress_example) {
        fprintf(stderr, "DFC FW version: %s\n", dfc_fx3_get_fw_version(&dfc));

        if (!(dfc_mode == DFC_MODE_UNKNOWN || dfc_mode == UART_ONLY)) {
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

        uint8_t current_dfc_mode = dfc_fx3_get_mode(&dfc);
        fprintf(stderr, "DFC mode: %hhu\n", current_dfc_mode);

        if (current_dfc_mode != dfc_mode) {
            fprintf(stderr, "[ERROR] Current DFC mode: %hhu - expected: %d\n", current_dfc_mode, dfc_mode);
            usb_close(&dfc.usb_device);
            return EXIT_FAILURE;
        }

        if (dfc_mode == SINGLE_ADC || dfc_mode == DUAL_ADC) {
            status = dfc_fx3_wakeup_adc(&dfc);
            if (status == -1) {
                usb_close(&dfc.usb_device);
                return EXIT_FAILURE;
            }

            status = dfc_fx3_shutdown_dac(&dfc);
            if (status == -1) {
                usb_close(&dfc.usb_device);
                return EXIT_FAILURE;
            }
        } else if (dfc_mode == DAC || dfc_mode == DAC_FX3_CLOCK) {
            status = dfc_fx3_shutdown_adc(&dfc);
            if (status == -1) {
                usb_close(&dfc.usb_device);
                return EXIT_FAILURE;
            }

            status = dfc_fx3_wakeup_dac(&dfc);
            if (status == -1) {
                usb_close(&dfc.usb_device);
                return EXIT_FAILURE;
            }
#define _FX3_CLOCK_TEST_
#ifdef _FX3_CLOCK_TEST_
        } else if (dfc_mode == SINGLE_ADC_FX3_CLOCK) {
            fprintf(stderr, "shutting down ADC\n");
            status = dfc_fx3_shutdown_adc(&dfc);
            if (status == -1) {
                usb_close(&dfc.usb_device);
                return EXIT_FAILURE;
            }
#endif  /* _FX3_CLOCK_TEST_ */
        }

        if (!(dfc_mode == SINGLE_ADC_FX3_CLOCK || dfc_mode == DAC_FX3_CLOCK)) {
            status = clock_start(&dfc.clock, &dfc.usb_device, reference_clock * (1.0 + 1e-6 * reference_ppm), samplerate);
            if (status == -1) {
                return EXIT_FAILURE;
            }
        }

        status = dfc_fx3_start(&dfc);
        if (status == -1) {
            usb_close(&dfc.usb_device);
            return EXIT_FAILURE;
        }
    }

    if (duration > 0) {
        stream_t stream;

        status = stream_init(&stream, stream_direction, stream_read_write_fileno, &dfc.usb_device, reqsize, queuedepth, show_histogram);
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

        stream_stats(&stream, duration);

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
    if (!(read_fileno == -1 || write_fileno == STDIN_FILENO)) {
        close(read_fileno);
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
