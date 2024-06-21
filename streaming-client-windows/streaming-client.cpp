#include "getopt.h"
#include <stdio.h>
#include <chrono>
#include <fstream>
#include <thread>

#include <wtypes.h>
#include <CyAPI.h>

// timer queue
#include <windows.h>


// useful types
typedef enum { STREAM_RX, STREAM_TX } streamDirection_t;

typedef enum {
    UART_ONLY,
    SINGLE_ADC,
    DUAL_ADC,
    DAC,
    SINGLE_ADC_FX3_CLOCK,
    DAC_FX3_CLOCK,
    DFC_MODE_UNKNOWN = -1
} dfcMode_t;

// useful constants
static const USHORT fx3_streamer_example[] = { 0x04b4, 0x00f1 };
static const USHORT fx3_dfu_mode[]         = { 0x04b4, 0x00f3 };
static const UCHAR GETFWVERSION  = 0x01;
static const UCHAR GETMODE       = 0x10;
static const UCHAR SETMODE       = 0x90;
static const UCHAR STARTADC      = 0xb2;
static const UCHAR STARTFX3      = 0xaa;
static const UCHAR STOPFX3       = 0xab;
static const UCHAR SHUTDOWNADC   = 0xc1;
static const UCHAR WAKEUPADC     = 0xc2;
static const UCHAR SHUTDOWNDAC   = 0xc3;
static const UCHAR WAKEUPDAC     = 0xc4;
static const ULONG TRANSFER_TIMEOUT = 100;  // transfer timeout in ms

// internal functions
static bool open_usb_device_with_vid_pid(CCyUSBDevice *usbDevice, USHORT vid, USHORT pid);
static bool usb_control_read(CCyUSBDevice *usbDevice, UCHAR request, const char *requestName, UCHAR *data, LONG dataSize);
static bool usb_control_write(CCyUSBDevice *usbDevice, UCHAR request, const char *requestName, UCHAR *data, LONG dataSize);
static int streamRxCallback(UINT8 *buffer, long length);
static int streamTxCallback(UINT8 *buffer, long length);
static void streamStats(streamDirection_t streamDirection, double elapsed);
static VOID CALLBACK doStopTransfers(PVOID lpParam, BOOLEAN TimerOrWaitFired);

// global variables
static UINT successCount = 0;               // number of successful transfers
static UINT failureCount = 0;               // number of failed transfers
static ULONGLONG totalTransferSize = 0;     // total size of data transfers
static SHORT sampleEvenMin = SHRT_MAX;  // minimum even sample value
static SHORT sampleEvenMax = SHRT_MIN;  // maximum even sample value
static SHORT sampleOddMin = SHRT_MAX;   // minimum odd sample value
static SHORT sampleOddMax = SHRT_MIN;   // maximum odd sample value

static const int SIXTEEN_BITS_SIZE = 65536;
static unsigned long long *histogramEven = NULL;   // histogram for even samples
static unsigned long long *histogramOdd = NULL;    // histogram for odd samples
static std::ofstream writeOstream;

static std::ifstream readIstream;
static uint8_t *readBuffer = NULL;

volatile bool stopTransfers = false;


int main(int argc, char *argv[])
{
    char *firmware_file = NULL;
    dfcMode_t dfc_mode = DFC_MODE_UNKNOWN;
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
            writeOstream.open(optarg, std::ios_base::out | std::ios_base::binary);
            if (!writeOstream) {
                fprintf(stderr, "open(%s) for writing failed\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'i':
            readIstream.open(optarg, std::ios_base::in | std::ios_base::binary);
            if (!readIstream) {
                fprintf(stderr, "open(%s) for reading failed\n", optarg);
                return EXIT_FAILURE;
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

    if (readIstream.is_open() && (writeOstream.is_open() || show_histogram)) {
        fprintf(stderr, "[ERROR] options -i (read from file) and -o (write to file) or -H (show histogram) are exclusive\n");
        fprintf(stderr, "[ERROR] streaming-client cannot not write and read at the same time (no full-duplex yet)\n");
        readIstream.close();
        writeOstream.close();
        return EXIT_FAILURE;
    }

    if (writeOstream.is_open() && show_histogram) {
        fprintf(stderr, "[ERROR] options -H (show histogram) and -o - (write to stdout) are mutually exclusive\n");
        writeOstream.close();
        return EXIT_FAILURE;
    }

    if (firmware_file == NULL) {
        fprintf(stderr, "missing firmware file\n");
        return EXIT_FAILURE;
    }

    streamDirection_t streamDirection = readIstream.is_open() ? STREAM_TX : STREAM_RX;
    if (dfc_mode == DFC_MODE_UNKNOWN) {
        switch (streamDirection) {
        case STREAM_RX:
            dfc_mode = SINGLE_ADC;
            break;
        case STREAM_TX:
            dfc_mode = DAC;
            break;
        }
    }

    if (streamDirection == STREAM_RX) {
        if (!(dfc_mode == SINGLE_ADC || dfc_mode == DUAL_ADC || dfc_mode == SINGLE_ADC_FX3_CLOCK)) {
            fprintf(stderr, "invalid DFC mode for RX stream direction\n");
            return EXIT_FAILURE;
        }
    } else if (streamDirection == STREAM_TX) {
        if (!(dfc_mode == DAC || dfc_mode == DAC_FX3_CLOCK)) {
            fprintf(stderr, "invalid DFC mode for TX stream direction\n");
            return EXIT_FAILURE;
        }
    }

    // look for streamer device first; if found, use that
    CCyUSBDevice *usbDevice = new CCyUSBDevice(NULL);
    if (!open_usb_device_with_vid_pid(usbDevice, fx3_streamer_example[0], fx3_streamer_example[1])) {
        fprintf(stderr, "FX3 streamer example not found - trying FX3 in DFU mode\n");
        CCyFX3Device *fx3Device = new CCyFX3Device();
        if (!open_usb_device_with_vid_pid(fx3Device, fx3_dfu_mode[0], fx3_dfu_mode[1])) {
            fprintf(stderr, "FX3 in DFU mode not found\n");
            return 1;
        }
        if (!fx3Device->IsBootLoaderRunning()) {
            fprintf(stderr, "Bootloader not running in FX3 in DFU mode\n");
            return 1;
        }
        fprintf(stderr, "upload FX3 firmware\n");
        FX3_FWDWNLOAD_ERROR_CODE status = fx3Device->DownloadFw(firmware_file, RAM);
        if (status != SUCCESS) {
            fprintf(stderr, "FX3 firmware upload failed - status=%d\n", status);
            return 1;
        }

        // look again for streamer device
        bool streamer_device_found = false;
        for (int retry = 0; retry < 10; retry++) {
            if (open_usb_device_with_vid_pid(usbDevice, fx3_streamer_example[0], fx3_streamer_example[1])) {
                streamer_device_found = true;
                fprintf(stderr, "FX3 firmware upload OK (retry=%d)\n", retry);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait 100ms before checking again
        }
        if (!streamer_device_found) {
            fprintf(stderr, "FX3 firmware upload failed - FX3 streamer example not found\n");
            return 1;
        }
    }

    if (!cypress_example) {
        // get FW version
        UCHAR fwVersion[64];
        if (!usb_control_read(usbDevice, GETFWVERSION, "GETFWVERSION", fwVersion, sizeof(fwVersion))) {
            return 1;
        }
        fprintf(stderr, "DFC FW version: %s\n", fwVersion);

        // set DFC mode
        UCHAR dfcMode = dfc_mode;
        if (!usb_control_write(usbDevice, SETMODE, "SETMODE", &dfcMode, sizeof(dfcMode))) {
            return 1;
        }

        /* wait a few ms before using the new mode */
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // get DFC mode
        uint8_t currentDfcMode = -2;
        if (!usb_control_read(usbDevice, GETMODE, "GETMODE", &currentDfcMode, sizeof(currentDfcMode))) {
            return 1;
        }
        fprintf(stderr, "DFC mode: %hhu\n", dfcMode);

        if (currentDfcMode != dfcMode) {
            fprintf(stderr, "[ERROR] Current DFC mode: %hhu - expected: %d\n", currentDfcMode, dfcMode);
            return 1;
        }

        if (dfcMode == SINGLE_ADC || dfcMode == DUAL_ADC) {
            // wakeup ADC0
            if (!usb_control_write(usbDevice, WAKEUPADC, "WAKEUPADC", NULL, 0)) {
                return 1;
            }

            // shutdown DAC
            if (!usb_control_write(usbDevice, SHUTDOWNDAC, "SHUTDOWNDAC", NULL, 0)) {
                return 1;
            }
        } else if (dfcMode == DAC || dfcMode == DAC_FX3_CLOCK) {
            // shutdown ADC0
            if (!usb_control_write(usbDevice, SHUTDOWNADC, "SHUTDOWNADC", NULL, 0)) {
                return 1;
            }

            // wakeup DAC
            if (!usb_control_write(usbDevice, WAKEUPDAC, "WAKEUPDAC", NULL, 0)) {
                return 1;
            }
#define _FX3_CLOCK_TEST_
#ifdef _FX3_CLOCK_TEST_
        } else if (dfcMode == SINGLE_ADC_FX3_CLOCK) {
            fprintf(stderr, "shutting down ADC\n");
            if (!usb_control_write(usbDevice, SHUTDOWNADC, "SHUTDOWNADC", NULL, 0)) {
                return 1;
            }
#endif  /* _FX3_CLOCK_TEST_ */
        }

        if (!(dfcMode == SINGLE_ADC_FX3_CLOCK || dfcMode == DAC_FX3_CLOCK)) {
            // start ADC clock
            double data[] = { reference_clock * (1.0 + 1e-6 * reference_ppm), samplerate };
            if (!usb_control_write(usbDevice, STARTADC, "STARTADC", (UCHAR *)data, sizeof(data))) {
                return 1;
            }
        }

        // start FX3
        if (!usb_control_write(usbDevice, STARTFX3, "STARTFX3", NULL, 0)) {
            return 1;
        }
    }

    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

    if (duration > 0) {
        // timer queue
        HANDLE timerQueue = CreateTimerQueue();
        if (timerQueue == NULL) {
            fprintf(stderr, "CreateTimerQueue failed - error=%d\n", GetLastError());
            return 1;
        }

        CCyBulkEndPoint *endPt = NULL;
        switch (streamDirection) {
        case STREAM_RX:
            endPt = usbDevice->BulkInEndPt;
            break;
        case STREAM_TX:
            endPt = usbDevice->BulkOutEndPt;
            break;
        }
        long pktSize = endPt->MaxPktSize;
        long transferSize = reqsize * pktSize;
        endPt->SetXferSize(transferSize);
        fprintf(stderr, "buffer transfer size = %ld - packet size = %ld\n", transferSize, pktSize);

        /* allocate read buffer if direction is STREAM_TX */
        if (streamDirection == STREAM_TX) {
            readBuffer = (uint8_t *)malloc(transferSize);
        }

        typedef struct {
            UINT8 *buffer;
            OVERLAPPED overlap;
            UCHAR *context;
        } transfer_t;

        // prepare transfers
        transfer_t *transfers = (transfer_t *) malloc(queuedepth * sizeof(transfer_t));
        for (int i = 0; i < queuedepth; i++) {
            transfer_t *transfer = &transfers[i];
            transfer->buffer = (UINT8 *) malloc(transferSize);
            ZeroMemory(&transfer->overlap, sizeof(OVERLAPPED));
            transfer->overlap.hEvent = CreateEvent(NULL, false, false, NULL);
            transfer->context = endPt->BeginDataXfer(transfer->buffer, transferSize, &transfer->overlap);
        }

        if (show_histogram) {
            histogramEven = (unsigned long long *) malloc(SIXTEEN_BITS_SIZE * sizeof(unsigned long long));
            histogramOdd = (unsigned long long *) malloc(SIXTEEN_BITS_SIZE * sizeof(unsigned long long));
            for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
                histogramEven[i] = 0;
            }
            for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
                histogramOdd[i] = 0;
            }
        }

        stopTransfers = false;

        HANDLE timer = NULL;
        if (!CreateTimerQueueTimer(&timer, timerQueue, (WAITORTIMERCALLBACK)doStopTransfers, NULL, duration * 1000, 0, 0)) {
            fprintf(stderr, "CreateTimerQueueTimer failed - error=%d\n", GetLastError());
            return 1;
        }

        // main transfer loop
        while (true) {
            bool doStopTransfers = stopTransfers;
            for (int i = 0; i < queuedepth; i++) {
                transfer_t *transfer = &transfers[i];
                if (!endPt->WaitForXfer(&transfer->overlap, TRANSFER_TIMEOUT)) {
                    fprintf(stderr, "WaitForXfer() timeout - NtStatus=0x%08x\n", endPt->NtStatus);
                    endPt->Abort();
                    stopTransfers = true;
                    failureCount++;
                    break;
                }
                if (!endPt->FinishDataXfer(transfer->buffer, transferSize, &transfer->overlap, transfer->context)) {
                    fprintf(stderr, "FinishDataXfer() failed - NtStatus=0x%08x\n", endPt->NtStatus);
                    endPt->Abort();
                    stopTransfers = true;
                    failureCount++;
                    break;
                }

                successCount++;
                switch (streamDirection) {
                case STREAM_RX:
                    if (streamRxCallback(transfer->buffer, transferSize) == -1) {
                        doStopTransfers = true;
                    }
                    break;
                case STREAM_TX:
                    if (streamTxCallback(transfer->buffer, transferSize) == -1) {
                        doStopTransfers = true;
                    }
                    break;
                }

                if (!doStopTransfers) {
                    // requeue the transfer
                    transfer->context = endPt->BeginDataXfer(transfer->buffer, transferSize, &transfer->overlap);
                    if (transfer->context == NULL) {
                        fprintf(stderr, "BeginDataXfer() failed - NtStatus=0x%08x\n", endPt->NtStatus);
                        endPt->Abort();
                        stopTransfers = true;
                        failureCount++;
                        break;
                    }
                }
            }

            if (doStopTransfers) {
                break;
            }
        }

        std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::microseconds> (end_time - start_time).count() / 1e6;

        streamStats(streamDirection, elapsed);

        // clean up transfers
        for (int i = 0; i < queuedepth; i++) {
            transfer_t *transfer = &transfers[i];
            CloseHandle(transfer->overlap.hEvent);
            free(transfer->buffer);
        }
        free(transfers);

        if (histogramEven != NULL) {
            free(histogramEven);
        }
        if (histogramOdd != NULL) {
            free(histogramOdd);
        }

        if (readBuffer != NULL) {
            free(readBuffer);
        }

        if (!DeleteTimerQueue(timerQueue)) {
            fprintf(stderr, "DeleteTimerQueue failed - error=%d\n", GetLastError());
        }
    }

    if (!cypress_example) {
        // stop FX3
        if (!usb_control_write(usbDevice, STOPFX3, "STOPFX3", NULL, 0)) {
            return 1;
        }
    }

    writeOstream.close();
    readIstream.close();

    return 0;
}


// internal functions
static bool open_usb_device_with_vid_pid(CCyUSBDevice *usbDevice, USHORT vid, USHORT pid)
{
    int nUsbDevices = usbDevice->DeviceCount();
    for (int i = 0; i < nUsbDevices; i++) {
        usbDevice->Open(i);
        if (usbDevice->VendorID == vid && usbDevice->ProductID == pid) {
            return true;
        }
    }
    usbDevice->Close();
    return false;
}

static bool usb_control_read(CCyUSBDevice *usbDevice, UCHAR request, const char *requestName, UCHAR *data, LONG dataSize)
{
    usbDevice->ControlEndPt->Target  = TGT_DEVICE;
    usbDevice->ControlEndPt->ReqType = REQ_VENDOR;
    usbDevice->ControlEndPt->ReqCode = request;
    usbDevice->ControlEndPt->Value   = 0;
    usbDevice->ControlEndPt->Index   = 0;
    if (!usbDevice->ControlEndPt->Read(data, dataSize)) {
        fprintf(stderr, "FX3 control request %s (0x%02hhx) failed\n", requestName, request);
        return false;
    }
    return true;
}

static bool usb_control_write(CCyUSBDevice *usbDevice, UCHAR request, const char *requestName, UCHAR *data, LONG dataSize)
{
    usbDevice->ControlEndPt->Target  = TGT_DEVICE;
    usbDevice->ControlEndPt->ReqType = REQ_VENDOR;
    usbDevice->ControlEndPt->ReqCode = request;
    usbDevice->ControlEndPt->Value   = 0;
    usbDevice->ControlEndPt->Index   = 0;
    if (!usbDevice->ControlEndPt->Write(data, dataSize)) {
        fprintf(stderr, "FX3 control request %s (0x%02hhx) failed\n", requestName, request);
        return false;
    }
    return true;
}

static int streamRxCallback(UINT8 *buffer, long length)
{
    totalTransferSize += length;
    SHORT *samples = (SHORT *)buffer;
    int nsamples = length / sizeof(samples[0]);
    for (int i = 0; i < nsamples; i++) {
        if (i % 2 == 0) {
            sampleEvenMin = samples[i] < sampleEvenMin ? samples[i] : sampleEvenMin;
            sampleEvenMax = samples[i] > sampleEvenMax ? samples[i] : sampleEvenMax;
        } else {
            sampleOddMin = samples[i] < sampleOddMin ? samples[i] : sampleOddMin;
            sampleOddMax = samples[i] > sampleOddMax ? samples[i] : sampleOddMax;
        }
    }

    if (histogramEven != NULL) {
        for (int i = 0; i < nsamples; i += 2) {
            histogramEven[samples[i] + SIXTEEN_BITS_SIZE / 2]++;
        }
    }
    if (histogramOdd != NULL) {
        for (int i = 1; i < nsamples; i += 2) {
            histogramOdd[samples[i] + SIXTEEN_BITS_SIZE / 2]++;
        }
    }

    if (writeOstream.is_open()) {
        //writeOstream.write((const UINT8 *)buffer, length);
        writeOstream.write((char *) buffer, length);
        if (!writeOstream) {
            fprintf(stderr, "write to output file failed\n");
            /* if there's any error stop writing to the output file */
            writeOstream.close();
            return -1;
        }
    }

    return 0;
}

static int streamTxCallback(UINT8 *buffer, long length)
{
    totalTransferSize += length;
    SHORT *samples = (SHORT *)buffer;

    if (!readIstream.is_open()) {
        return -1;
    }
    //readIstream.read((const UINT8 *)readBuffer, length);
    readIstream.read((char *) readBuffer, length);
    if (!readIstream) {
        if (readIstream.eof()) {
            /* EOF - send a message and exit */
            fprintf(stderr, "EOF from input file. Done streaming\n");
            readIstream.close();
            return -1;
        }
        fprintf(stderr, "read from inpout file failed\n");
        /* if there's any error stop reading from the input file */
        readIstream.close();
        return -1;
    }

    /* shift the values by 2 bits because the DAC is comnnected to bits 2:15 */
    short *readSamples = (short *)readBuffer;
    int nReadSamples = readIstream.gcount() / sizeof(readSamples[0]);
    for (int i = 0; i < nReadSamples; i++) {
        samples[i] = readSamples[i] << 2;
    }

    totalTransferSize += nReadSamples * sizeof(readSamples[0]);

    return 0;
}

static void streamStats(streamDirection_t streamDirection, double elapsed)
{
    fprintf(stderr, "success count: %u\n", successCount);
    fprintf(stderr, "failure count: %u\n", failureCount);
    fprintf(stderr, "transfer size: %llu B\n", totalTransferSize);
    fprintf(stderr, "transfer rate: %.0lf kB/s\n", (double) totalTransferSize / elapsed / 1024.0);
    if (streamDirection == STREAM_RX) {
        fprintf(stderr, "even samples range: [%hd,%hd]\n", sampleEvenMin, sampleEvenMax);
        fprintf(stderr, "odd samples range: [%hd,%hd]\n", sampleOddMin, sampleOddMax);

        if (histogramEven != NULL) {
            int histogramMin = -1;
            int histogramMax = -1;
            unsigned long long totalHistogramSamples = 0;
            for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
                if (histogramEven[i] > 0) {
                    if (histogramMin < 0) {
                        histogramMin = i;
                    }
                    histogramMax = i;
                    totalHistogramSamples += histogramEven[i];
                }
            }
            if (totalHistogramSamples > 0) {
                fprintf(stdout, "# Even samples histogram\n");
                for (int i = histogramMin; i <= histogramMax; i++) {
                    fprintf(stdout, "%d\t%llu\n", i - SIXTEEN_BITS_SIZE / 2,
                            histogramEven[i]);
                }
                fprintf(stdout, "\n");
            }
            fprintf(stderr, "total even histogram samples: %llu\n", totalHistogramSamples);
        }

        if (histogramOdd != NULL) {
            int histogramMin = -1;
            int histogramMax = -1;
            unsigned long long totalHistogramSamples = 0;
            for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
                if (histogramOdd[i] > 0) {
                    if (histogramMin < 0) {
                        histogramMin = i;
                    }
                    histogramMax = i;
                    totalHistogramSamples += histogramOdd[i];
                }
            }
            if (totalHistogramSamples > 0) {
                fprintf(stdout, "# Odd samples histogram\n");
                for (int i = histogramMin; i <= histogramMax; i++) {
                    fprintf(stdout, "%d\t%llu\n", i - SIXTEEN_BITS_SIZE / 2,
                            histogramOdd[i]);
                }
                fprintf(stdout, "\n");
            }
            fprintf(stderr, "total odd histogram samples: %llu\n", totalHistogramSamples);
        }
    }

    return;
}

static VOID CALLBACK doStopTransfers(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
    stopTransfers = true;
}
