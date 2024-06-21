//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* typedefs */
typedef struct {
    double value;
} constant_waveform_t;

typedef struct {
    int frequency_numerator;
    int frequency_denominator;
    double amplitude;
    double initial_phase;
} sine_waveform_t;

typedef struct {
    int frequency_numerator;
    int frequency_denominator;
    double amplitude;
    double duty_cycle;
    double initial_offset;
} square_waveform_t;

typedef struct {
    int frequency_numerator;
    int frequency_denominator;
    double amplitude;
    double initial_offset;
} triangular_waveform_t;

typedef struct {
    int frequency_numerator;
    int frequency_denominator;
    double amplitude;
    int flow_numerator;
    int flow_denominator;
    int fhigh_numerator;
    int fhigh_denominator;
    double initial_phase;
} sweep_waveform_t;

/* internal functions */
static int gcd(int a, int b);
static int lcm(int a, int b);

int main(int argc, char *argv[])
{
    enum { MAX_WAVEFORMS = 10 };
    enum { MAX_PERIOD_LENGTH = 1000000000 };

    constant_waveform_t constant_waveforms[MAX_WAVEFORMS];
    int num_constant_waveforms = 0;
    sine_waveform_t sine_waveforms[MAX_WAVEFORMS];
    int num_sine_waveforms = 0;
    square_waveform_t square_waveforms[MAX_WAVEFORMS];
    int num_square_waveforms = 0;
    triangular_waveform_t triangular_waveforms[MAX_WAVEFORMS];
    int num_triangular_waveforms = 0;
    sweep_waveform_t sweep_waveforms[MAX_WAVEFORMS];
    int num_sweep_waveforms = 0;

    int min_value = -8192;
    int max_value =  8191;

    int buffer_size = 262144;
    unsigned long long num_samples = 0;
    int output_file = STDOUT_FILENO;

    int opt;
    while ((opt = getopt(argc, argv, "c:s:q:t:w:m:b:n:o:")) != -1) {
        switch (opt) {
        case 'c':
            if (num_constant_waveforms >= MAX_WAVEFORMS) {
                fprintf(stderr, "too many constant waveforms\n");
                return EXIT_FAILURE;
            }
            {
                constant_waveform_t *cw = &constant_waveforms[num_constant_waveforms];
                if (sscanf(optarg, "%lf", &cw->value) != 1) {
                    fprintf(stderr, "invalid constant waveform specification: %s\n", optarg);
                    return EXIT_FAILURE;
                }
            }
            num_constant_waveforms++;
            break;
        case 's':
            if (num_sine_waveforms >= MAX_WAVEFORMS) {
                fprintf(stderr, "too many sine waveforms\n");
                return EXIT_FAILURE;
            }
            {
                sine_waveform_t *sw = &sine_waveforms[num_sine_waveforms];
                int nparams = sscanf(optarg, "%d/%d,%lf,%lf", &sw->frequency_numerator, &sw->frequency_denominator, &sw->amplitude, &sw->initial_phase);
                if (!(nparams == 3 || nparams == 4)) {
                    fprintf(stderr, "invalid sine waveform specification: %s\n", optarg);
                    return EXIT_FAILURE;
                }
            }
            num_sine_waveforms++;
            break;
        case 'q':
            if (num_square_waveforms >= MAX_WAVEFORMS) {
                fprintf(stderr, "too many square waveforms\n");
                return EXIT_FAILURE;
            }
            {
                square_waveform_t *qw = &square_waveforms[num_square_waveforms];
                int nparams = sscanf(optarg, "%d/%d,%lf,%lf,%lf", &qw->frequency_numerator, &qw->frequency_denominator, &qw->amplitude, &qw->duty_cycle, &qw->initial_offset);
                if (!(nparams == 3 || nparams == 4 || nparams == 5)) {
                    fprintf(stderr, "invalid square waveform specification: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                if (nparams == 3) {
                    qw->duty_cycle = 0.5;
                }
            }
            num_square_waveforms++;
            break;
        case 't':
            if (num_triangular_waveforms >= MAX_WAVEFORMS) {
                fprintf(stderr, "too many triangular waveforms\n");
                return EXIT_FAILURE;
            }
            {
                triangular_waveform_t *tw = &triangular_waveforms[num_triangular_waveforms];
                int nparams = sscanf(optarg, "%d/%d,%lf,%lf", &tw->frequency_numerator, &tw->frequency_denominator, &tw->amplitude, &tw->initial_offset);
                if (!(nparams == 3 || nparams == 4)) {
                    fprintf(stderr, "invalid triangular waveform specification: %s\n", optarg);
                    return EXIT_FAILURE;
                }
            }
            num_triangular_waveforms++;
            break;
        case 'w':
            if (num_sweep_waveforms >= MAX_WAVEFORMS) {
                fprintf(stderr, "too many sweep waveforms\n");
                return EXIT_FAILURE;
            }
            {
                sweep_waveform_t *ww = &sweep_waveforms[num_sweep_waveforms];
                int nparams = sscanf(optarg, "%d/%d,%lf,%d/%d,%d/%d,%lf", &ww->frequency_numerator, &ww->frequency_denominator, &ww->amplitude, &ww->flow_numerator, &ww->flow_denominator, &ww->fhigh_numerator, &ww->fhigh_denominator, &ww->initial_phase);
                if (!(nparams == 7 || nparams == 8)) {
                    fprintf(stderr, "invalid sweep waveform specification: %s\n", optarg);
                    return EXIT_FAILURE;
                }
            }
            num_sweep_waveforms++;
            break;
        case 'm':
            if (sscanf(optarg, "%d:%d", &min_value, &max_value) != 2) {
                fprintf(stderr, "invalid min:max values: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'b':
            if (sscanf(optarg, "%d", &buffer_size) != 1) {
                fprintf(stderr, "invalid buffer size value: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'n':
            if (sscanf(optarg, "%llu", &num_samples) != 1) {
                fprintf(stderr, "invalid num_samples value: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'o':
            if (strcmp(optarg, "-") == 0) {
                output_file = STDOUT_FILENO;
            } else {
                output_file = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_file == -1) {
                    fprintf(stderr, "open(%s) for writing failed: %s\n", optarg, strerror(errno));
                    return EXIT_FAILURE;
                }
            }
            break;
        case '?':
            /* invalid option */
            return EXIT_FAILURE;
        }
    }

    int period_length = 1;
    for (int i = 0; i < num_sine_waveforms; i++) {
        period_length = lcm(period_length, sine_waveforms[i].frequency_denominator);
        if (period_length > MAX_PERIOD_LENGTH) {
            fprintf(stderr, "period too long - choose different denominators\n");
            return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < num_square_waveforms; i++) {
        period_length = lcm(period_length, square_waveforms[i].frequency_denominator);
        if (period_length > MAX_PERIOD_LENGTH) {
            fprintf(stderr, "period too long - choose different denominators\n");
            return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < num_triangular_waveforms; i++) {
        period_length = lcm(period_length, triangular_waveforms[i].frequency_denominator);
        if (period_length > MAX_PERIOD_LENGTH) {
            fprintf(stderr, "period too long - choose different denominators\n");
            return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < num_sweep_waveforms; i++) {
        int sweep_lcm = lcm(sweep_waveforms[i].frequency_denominator, sweep_waveforms[i].flow_denominator);
        sweep_lcm = lcm(sweep_lcm, sweep_waveforms[i].fhigh_denominator);
        period_length = lcm(period_length, sweep_lcm);
        if (period_length > MAX_PERIOD_LENGTH) {
            fprintf(stderr, "period too long - choose different denominators\n");
            return EXIT_FAILURE;
        }
    }
    fprintf(stderr, "period length: %d\n", period_length);

    fprintf(stderr, "pre-computing waveform\n");

    double *waveform = (double *) malloc(period_length * sizeof(double));

    /* DC components */
    double dc = 0;
    for (int i = 0; i < num_constant_waveforms; i++) {
        constant_waveform_t *cw = &constant_waveforms[i];
        dc += cw->value;
    }
    if (dc < min_value || dc > max_value) {
        fprintf(stderr, "DC component out of range: %lf\n", dc);
        return EXIT_FAILURE;
    }
    for (int i = 0; i < period_length; i++) {
        waveform[i] = dc;
    }

    /* sine wave components */
    for (int i = 0; i < num_sine_waveforms; i++) {
        sine_waveform_t *sw = &sine_waveforms[i];
        int fd = sw->frequency_denominator;
        double *swaveform = (double *) malloc(fd * sizeof(double));
        double phase_offset = sw->initial_phase * M_PI / 180.0;
        double delta_phase = 2.0 * M_PI * (double) sw->frequency_numerator / (double) fd;
        for (int j = 0; j < fd; j++) {
            swaveform[j] = sw->amplitude * sin(phase_offset + delta_phase * j);
        }
        for (int j = 0; j < period_length; j++) {
            waveform[j] += swaveform[j % fd];
        }
        free(swaveform);
    }

    /* square wave components */
    for (int i = 0; i < num_square_waveforms; i++) {
        square_waveform_t *qw = &square_waveforms[i];
        int fd = qw->frequency_denominator;
        double *qwaveform = (double *) malloc(fd * sizeof(double));
        int offset = (int) ((1.0 - qw->initial_offset) * fd) % fd;
        int duty_cycle = qw->duty_cycle * fd;
        for (int j = 0; j < fd; j++) {
            int jj = (j + offset) * qw->frequency_numerator % fd;
            if (jj < duty_cycle) {
                qwaveform[j] = qw->amplitude;
            } else {
                qwaveform[j] = -qw->amplitude;
            }
        }
        for (int j = 0; j < period_length; j++) {
            waveform[j] += qwaveform[j % fd];
        }
        free(qwaveform);
    }

    /* triangular wave components */
    for (int i = 0; i < num_triangular_waveforms; i++) {
        triangular_waveform_t *tw = &triangular_waveforms[i];
        int fd = tw->frequency_denominator;
        double *twaveform = (double *) malloc(fd * sizeof(double));
        /* start the triangular wave from near 0 (when initial_offest is 0) */
        int offset = (int) ((1.25 - tw->initial_offset) * fd) % fd;
        for (int j = 0; j < fd; j++) {
            int jj = (j + offset) * tw->frequency_numerator % fd;
            if (jj < fd / 2) {
                twaveform[j] = tw->amplitude * (-1.0 + 4.0 * jj / fd);
            } else {
                twaveform[j] = tw->amplitude * (3.0 - 4.0 * jj / fd); 
            }
        }
        for (int j = 0; j < period_length; j++) {
            waveform[j] += twaveform[j % fd];
        }
        free(twaveform);
    }

    /* sweep wave components */
    for (int i = 0; i < num_sweep_waveforms; i++) {
        sweep_waveform_t *ww = &sweep_waveforms[i];
        int sweep_lcm = lcm(ww->frequency_denominator, ww->flow_denominator);
        sweep_lcm = lcm(sweep_lcm, ww->fhigh_denominator);
        double *wwaveform = (double *) malloc(sweep_lcm * sizeof(double));
        double phase_offset = ww->initial_phase * M_PI / 180.0;
        double half_period = ((double) ww->frequency_denominator / (double) ww->frequency_numerator) / 2.0;
        double delta_phase_low = 2.0 * M_PI * half_period * (double) ww->flow_numerator / (double) ww->flow_denominator;
        double delta_phase_high = 2.0 * M_PI * half_period * (double) ww->fhigh_numerator / (double) ww->fhigh_denominator;
        double delta_omega = delta_phase_high - delta_phase_low;
        double delta_phase_half_period = fmod((delta_phase_low + delta_phase_high) / 2.0, 2.0 * M_PI);

        for (int j = 0; j < sweep_lcm; j++) {
            double half_period_int;
            double half_period_frac = modf((double) j / half_period, &half_period_int);
            double phase = fmod(phase_offset + half_period_int * delta_phase_half_period, 2.0 * M_PI);
            if ((int) half_period_int % 2 == 0) {
                /* increasing frequency half period */
                phase += (delta_phase_low + delta_omega * half_period_frac) * half_period_frac;
            } else {
                /* decreasing frequency half period */
                phase += (delta_phase_high - delta_omega * half_period_frac) * half_period_frac;
            }
            wwaveform[j] = ww->amplitude * sin(phase);
        }
        for (int j = 0; j < period_length; j++) {
            waveform[j] += wwaveform[j % sweep_lcm];
        }
        free(wwaveform);
    }

#if 0
    for (int i = 0; i < period_length; i++) {
        printf("%3d\t%lf\n", i, waveform[i]);
    }
    exit(0);
#endif

    /* convert to shorts and make sure the values are inside the boundaries */
    short *samples = (short *) malloc(period_length * sizeof(short));
    int num_overflows = 0;
    for (int i = 0; i < period_length; i++) {
        long sample = lrint(waveform[i]);
        if (sample < min_value) {
            sample = min_value;
            num_overflows++;
        } else if (sample > max_value) {
            sample = max_value;
            num_overflows++;
        }
        samples[i] = sample;
    }
    free(waveform);
    if (num_overflows > 0) {
        fprintf(stderr, "warning - overflow/underflow condition for %d samples\n", num_overflows);
    }

#if 0
    for (int i = 0; i < period_length; i++) {
        printf("%3d\t%d\n", i, samples[i]);
    }
#endif

    fprintf(stderr, "sending waveform to output\n");

    short *buffer = (short *) malloc(buffer_size * sizeof(short));

    int samples_start = 0;
    unsigned long long remaining = num_samples;
    while (1) {
        /* fill buffer */
        int buffer_last = buffer_size;
        if (remaining && remaining < (unsigned long long) buffer_size) {
            buffer_last = remaining;
        }
        int buffer_start = 0;
        while (buffer_start < buffer_last) {
            int samples_last = buffer_last - buffer_start + samples_start;
            if (samples_last > period_length) {
                samples_last = period_length;
            }
            int samples_copied = samples_last - samples_start;
            memcpy(&buffer[buffer_start], &samples[samples_start], samples_copied * sizeof(short));
            samples_start += samples_copied;
            if (samples_start == period_length) {
                samples_start = 0;
            }
            buffer_start += samples_copied;
        }

        /* write out buffer */
        size_t length = buffer_last * sizeof(short);
        size_t write_remaining = length;
        while (write_remaining > 0) {
            ssize_t written = write(output_file, buffer + (length - write_remaining), write_remaining);
            if (written == -1) {
                fprintf(stderr, "write to output file failed - error: %s\n", strerror(errno));
                /* if there's any error stop writing to output file */
                return EXIT_FAILURE;
            } else {
                write_remaining -= written;
            }
        }

        if (remaining) {
            remaining -= buffer_last;
            if (remaining == 0) {
                break;
            }
        }
    }

    free(buffer);
    free(samples);
    if (output_file != STDOUT_FILENO) {
        close(output_file);
    }

    return EXIT_SUCCESS;
}

/* internal functions */
static int gcd(int a, int b) {
    while (1) {
        int c = a % b;
        if (c == 0)
            return b;
        a = b;
        b = c;
    }
}

static int lcm(int a, int b) {
    return (a / gcd(a, b)) * b;
}
