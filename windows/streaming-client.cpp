#include "getopt.h"
#include <stdio.h>
#include <chrono>
#include <thread>

#include <wtypes.h>
#include <CyAPI.h>

// timer queue
#include <windows.h>


// useful constants
static const USHORT fx3_streamer_example[] = { 0x04b4, 0x00f1 };
static const USHORT fx3_dfu_mode[]         = { 0x04b4, 0x00f3 };
static const UCHAR STARTADC = 0xb2;
static const UCHAR STARTFX3 = 0xaa;
static const UCHAR STOPFX3  = 0xab;
static const ULONG TRANSFER_TIMEOUT = 100;  // transfer timeout in ms

// internal functions
static bool open_usb_device_with_vid_pid(CCyUSBDevice *usbDevice, USHORT vid, USHORT pid);
static void streamCallback(UINT8 *buffer, long length);
static void streamStats(unsigned int duration);
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

volatile bool stopTransfers = false;


int main(int argc, char *argv[])
{
    char *firmware_file = NULL;
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

    int opt;
    while ((opt = getopt(argc, argv, "f:s:x:c:i:e:r:q:t:CH")) != -1) {
        switch (opt) {
        case 'f':
            firmware_file = optarg;
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
        case 'C':
            cypress_example = true;
            break;
        case 'H':
            show_histogram = true;
            break;
        }
    }

    if (firmware_file == NULL) {
        fprintf(stderr, "missing firmware file\n");
        return EXIT_FAILURE;
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

         // loog again for streamer device
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

    // timer queue
    HANDLE timerQueue = CreateTimerQueue();
    if (timerQueue == NULL) {
        fprintf(stderr, "CreateTimerQueue failed - error=%d\n", GetLastError());
        return 1;
    }

    CCyBulkEndPoint *endPt = usbDevice->BulkInEndPt;
    long pktSize = endPt->MaxPktSize;
    long transferSize = reqsize * pktSize;
    endPt->SetXferSize(transferSize);
    fprintf(stderr, "buffer transfer size = %ld - packet size = %ld\n", transferSize, pktSize);

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

    if (!cypress_example) {
        // start ADC clock
        usbDevice->ControlEndPt->Target  = TGT_DEVICE;
        usbDevice->ControlEndPt->ReqType = REQ_VENDOR;
        usbDevice->ControlEndPt->ReqCode = STARTADC;
        usbDevice->ControlEndPt->Value   = 0;
        usbDevice->ControlEndPt->Index   = 0;
        double data[] = { reference_clock * (1.0 + 1e-6 * reference_ppm), samplerate };
        LONG dataSize = sizeof(data);
        if (!usbDevice->ControlEndPt->Write((UCHAR *)data, dataSize)) {
            fprintf(stderr, "FX3 control STARTADC command failed\n");
            return 1;
        }

        // start FX3
        usbDevice->ControlEndPt->Target  = TGT_DEVICE;
        usbDevice->ControlEndPt->ReqType = REQ_VENDOR;
        usbDevice->ControlEndPt->ReqCode = STARTFX3;
        usbDevice->ControlEndPt->Value   = 0;
        usbDevice->ControlEndPt->Index   = 0;
        dataSize = 0;
        if (!usbDevice->ControlEndPt->Write(NULL, dataSize)) {
            fprintf(stderr, "FX3 control STARTFX3 command failed\n");
            return 1;
        }
    }

    stopTransfers = false;

    HANDLE timer = NULL;
    if (duration > 0) {
        if (!CreateTimerQueueTimer(&timer, timerQueue, (WAITORTIMERCALLBACK)doStopTransfers, NULL, duration * 1000, 0, 0)) {
            fprintf(stderr, "CreateTimerQueueTimer failed - error=%d\n", GetLastError());
            return 1;
        }
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
            streamCallback(transfer->buffer, transferSize);

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

    streamStats(duration);

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

    if (!cypress_example) {
        // stop FX3
        usbDevice->ControlEndPt->Target  = TGT_DEVICE;
        usbDevice->ControlEndPt->ReqType = REQ_VENDOR;
        usbDevice->ControlEndPt->ReqCode = STOPFX3;
        usbDevice->ControlEndPt->Value   = 0;
        usbDevice->ControlEndPt->Index   = 0;
        LONG dataSize = 0;
        if (!usbDevice->ControlEndPt->Write(NULL, dataSize)) {
            fprintf(stderr, "FX3 control STOPFX3 command failed\n");
            return 1;
        }
    }

    if (!DeleteTimerQueue(timerQueue)) {
        fprintf(stderr, "DeleteTimerQueue failed - error=%d\n", GetLastError());
    }

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

static void streamCallback(UINT8 *buffer, long length)
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

    return;
}

static void streamStats(unsigned int duration)
{
    fprintf(stderr, "success count: %u\n", successCount);
    fprintf(stderr, "failure count: %u\n", failureCount);
    fprintf(stderr, "transfer size: %llu B\n", totalTransferSize);
    fprintf(stderr, "transfer rate: %.0lf kB/s\n", (double) totalTransferSize / duration / 1024.0);
    fprintf(stderr, "even samples range: [%hd,%hd]\n", sampleEvenMin, sampleEvenMax);
    fprintf(stderr, "odd samples range: [%hd,%hd]\n", sampleOddMin, sampleOddMax);

    if (histogramEven != NULL) {
        int histogramMin = SIXTEEN_BITS_SIZE - 1;
        int histogramMax = 0;
        unsigned long long totalHistogramSamples = 0;
        for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
            if (histogramEven[i] > 0) {
                histogramMin = i < histogramMin ? i : histogramMin;
                histogramMax = i > histogramMax ? i : histogramMax;
                totalHistogramSamples += histogramEven[i];
            }
        }
        if (histogramMax >= histogramMin) {
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
        int histogramMin = SIXTEEN_BITS_SIZE - 1;
        int histogramMax = 0;
        unsigned long long totalHistogramSamples = 0;
        for (int i = 0; i < SIXTEEN_BITS_SIZE; i++) {
            if (histogramOdd[i] > 0) {
                histogramMin = i < histogramMin ? i : histogramMin;
                histogramMax = i > histogramMax ? i : histogramMax;
                totalHistogramSamples += histogramOdd[i];
            }
        }
        if (histogramMax >= histogramMin) {
            fprintf(stdout, "# Odd samples histogram\n");
            for (int i = histogramMin; i <= histogramMax; i++) {
                fprintf(stdout, "%d\t%llu\n", i - SIXTEEN_BITS_SIZE / 2,
                        histogramOdd[i]);
            }
            fprintf(stdout, "\n");
        }
        fprintf(stderr, "total odd histogram samples: %llu\n", totalHistogramSamples);
    }

    return;
}

static VOID CALLBACK doStopTransfers(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
    stopTransfers = true;
}
