//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "cyu3uart.h"
#include "cyu3i2c.h"
#include "cyu3gpio.h"
#include "cyu3utils.h"
#include "cyu3pib.h"
#include "cyu3gpif.h"

#include "app.h"
#include "dfc_modes.h"
#include "si5351.h"
#include "usb.h"

CyU3PThread glAppThread;            /* Application thread structure */
uint8_t glCurrentDFCModeIndex;      /* Current DFC operating mode index */
DFCMode *glCurrentDFCMode;          /* Current DFC operating mode */

CyBool_t glIsApplnActive = CyFalse;     /* Whether the application is active or not. */
CyBool_t glForceLinkU2   = CyFalse;     /* Whether the device should try to initiate U2 mode. */

uint8_t glEp0Buffer[64] __attribute__ ((aligned (32))); /* Local buffer used for vendor command handling. */
volatile uint8_t glSetDFCModeRqt = -1;
volatile CyBool_t glStartFx3Rqt = CyFalse;
volatile CyBool_t glStopFx3Rqt = CyFalse;
volatile CyBool_t glStartAdcRqt = CyFalse;
volatile double glStartAdcReference = 0.0;
volatile double glStartAdcFrequency = 0.0;

/* internal functions */
static void SetDFCMode(uint8_t dfcModeIndex);
static void StartFx3();
static void StopFx3();

/* Application Error Handler */
void
CyFxAppErrorHandler (
        CyU3PReturnStatus_t apiRetStatus    /* API return status */
        )
{
    /* Application failed with the error code apiRetStatus */

    /* Add custom debug or recovery actions here */

    /* Let's do a device reset here. */
    CyU3PThreadSleep (100);
    CyU3PDeviceReset (CyFalse);

    /* Loop Indefinitely */
    for (;;)
    {
        /* Thread sleep : 100 ms */
        CyU3PThreadSleep (100);
    }
}

/* This function starts the application. This is called
 * when a SET_CONF event is received from the USB host. The endpoints
 * are configured and the DMA pipe is setup in this function. */
void
CyFxApplnStart (
        void)
{
    uint16_t size = 0;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();

    /* Based on the Bus Speed configure the endpoint packet size */
    switch (usbSpeed)
    {
    case CY_U3P_FULL_SPEED:
        size = 64;
        break;

    case CY_U3P_HIGH_SPEED:
        size = 512;
        break;

    case  CY_U3P_SUPER_SPEED:
        /* Disable USB link low power entry to optimize USB throughput. */
        CyU3PUsbLPMDisable();
        size = 1024;
        break;

    default:
        CyU3PDebugPrint (4, "Error! Invalid USB speed.\r\n");
        CyFxAppErrorHandler (CY_U3P_ERROR_FAILURE);
        break;
    }

    if (glCurrentDFCMode->Start != NULL)
        glCurrentDFCMode->Start(usbSpeed, size);

    /* Update the flag so that the application thread is notified of this. */
    glIsApplnActive = CyTrue;
}

/* This function stops the application. This shall be called whenever a RESET
 * or DISCONNECT event is received from the USB host. */
void
CyFxApplnStop (
        void)
{
    /* Update the flag so that the application thread is notified of this. */
    glIsApplnActive = CyFalse;

    if (glCurrentDFCMode->Stop != NULL)
        glCurrentDFCMode->Stop();
}

/* Callback to handle the USB setup requests. */
CyBool_t
CyFxApplnUSBSetupCB (
        uint32_t setupdat0, /* SETUP Data 0 */
        uint32_t setupdat1  /* SETUP Data 1 */
    )
{
    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function.
     * This application does not support any class or vendor requests. */

    uint8_t  bRequest, bReqType;
    uint8_t  bType, bTarget;
    uint16_t wValue, wIndex, wLength;
    CyBool_t isHandled = CyFalse;
    CyU3PReturnStatus_t status;

    /* FX3 fw version is just the build timestamp for now */
    uint8_t fwVersion[] = __TIMESTAMP__;

    /* Decode the fields from the setup request. */
    bReqType = (setupdat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
    bType    = (bReqType & CY_U3P_USB_TYPE_MASK);
    bTarget  = (bReqType & CY_U3P_USB_TARGET_MASK);
    bRequest = ((setupdat0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
    wValue   = ((setupdat0 & CY_U3P_USB_VALUE_MASK)   >> CY_U3P_USB_VALUE_POS);
    wIndex   = ((setupdat1 & CY_U3P_USB_INDEX_MASK)   >> CY_U3P_USB_INDEX_POS);
    wLength  = ((setupdat1 & CY_U3P_USB_LENGTH_MASK)  >> CY_U3P_USB_LENGTH_POS);

    if (bType == CY_U3P_USB_VENDOR_RQT)
    {
        /* Vendor command is sent by test applications. Start the loop that tries to keep the link
         * in U0.
         */
        switch (bRequest)
        {
            case 0x01: /* DFC - GETFWVERSION */
                status = CyU3PUsbSendEP0Data (sizeof(fwVersion), fwVersion);
                if (status != CY_U3P_SUCCESS)
                {
                    CyU3PDebugPrint (2, "Send data failed\r\n");
                }
                CyU3PUsbAckSetup ();
                isHandled = CyTrue; 
                break;

            case 0x10: /* DFC - GETMODE */
                status = CyU3PUsbSendEP0Data (sizeof(glCurrentDFCModeIndex), &glCurrentDFCModeIndex);
                if (status != CY_U3P_SUCCESS)
                {
                    CyU3PDebugPrint (2, "Send data failed\r\n");
                }
                CyU3PUsbAckSetup ();
                isHandled = CyTrue; 
                break;

            case 0x90: /* DFC - SETMODE */
                status = CyU3PUsbGetEP0Data (wLength, glEp0Buffer, NULL);
                if (status != CY_U3P_SUCCESS)
                {
                    CyU3PDebugPrint (2, "Get data failed\r\n");
                }
                uint8_t dfcModeIndex = glEp0Buffer[0];
                if (dfcModeIndex < NumDFCModes && dfcModeIndex != glCurrentDFCModeIndex) {
                    glSetDFCModeRqt = dfcModeIndex;
                }
                CyU3PUsbAckSetup ();
                isHandled = CyTrue;
                break;

            case 0xAA: /* DFC - STARTFX3 */
                glStartFx3Rqt = CyTrue;
                CyU3PUsbAckSetup ();
                isHandled = CyTrue;
                break;

            case 0xAB: /* DFC - STOPFX3 */
                glStopFx3Rqt = CyTrue;
                CyU3PUsbAckSetup ();
                isHandled = CyTrue;
                break;

            case 0xB2: /* DFC - STARTADC */
                status = CyU3PUsbGetEP0Data (wLength, glEp0Buffer, NULL);
                if (status != CY_U3P_SUCCESS)
                {
                    CyU3PDebugPrint (2, "Get data failed\r\n");
                }
                glStartAdcRqt = CyTrue;
                double *ep0_data = (double *)glEp0Buffer;
                glStartAdcReference = ep0_data[0];
                glStartAdcFrequency = ep0_data[1];
                isHandled = CyTrue;
                break;

            default:
                break;
        }
    }

    if (bType == CY_U3P_USB_STANDARD_RQT)
    {
        /* Handle SET_FEATURE(FUNCTION_SUSPEND) and CLEAR_FEATURE(FUNCTION_SUSPEND)
         * requests here. It should be allowed to pass if the device is in configured
         * state and failed otherwise. */
        if ((bTarget == CY_U3P_USB_TARGET_INTF) && ((bRequest == CY_U3P_USB_SC_SET_FEATURE)
                    || (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)) && (wValue == 0))
        {
            if (glIsApplnActive)
            {
                CyU3PUsbAckSetup ();

                /* As we have only one interface, the link can be pushed into U2 state as soon as
                   this interface is suspended.
                 */
                if (bRequest == CY_U3P_USB_SC_SET_FEATURE)
                {
                    glForceLinkU2 = CyTrue;
                }
                else
                {
                    glForceLinkU2 = CyFalse;
                }
            }
            else
                CyU3PUsbStall (0, CyTrue, CyFalse);

            isHandled = CyTrue;
        }

        /* CLEAR_FEATURE request for endpoint is always passed to the setup callback
         * regardless of the enumeration model used. When a clear feature is received,
         * the previous transfer has to be flushed and cleaned up. This is done at the
         * protocol level. Since this is just a loopback operation, there is no higher
         * level protocol. So flush the EP memory and reset the DMA channel associated
         * with it. If there are more than one EP associated with the channel reset both
         * the EPs. The endpoint stall and toggle / sequence number is also expected to be
         * reset. Return CyFalse to make the library clear the stall and reset the endpoint
         * toggle. Or invoke the CyU3PUsbStall (ep, CyFalse, CyTrue) and return CyTrue.
         * Here we are clearing the stall. */
        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) && (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)
                && (wValue == CY_U3P_USBX_FS_EP_HALT))
        {
            if (glIsApplnActive)
            {
                if (wIndex == CY_FX_EP_CONSUMER)
                {
                    if (glCurrentDFCMode->Reset != NULL)
                        glCurrentDFCMode->Reset();
                    isHandled = CyTrue;
                    CyU3PUsbAckSetup ();
                }
            }
        }
    }

    return isHandled;
}

/* This is the callback function to handle the USB events. */
void
CyFxApplnUSBEventCB (
    CyU3PUsbEventType_t evtype, /* Event type */
    uint16_t            evdata  /* Event data */
    )
{
    if ((evtype != CY_U3P_USB_EVENT_EP0_STAT_CPLT) && (evtype != CY_U3P_USB_EVENT_RESUME))
        CyU3PDebugPrint (2, "USB event: %d %d\r\n", evtype, evdata);

    switch (evtype)
    {
        case CY_U3P_USB_EVENT_CONNECT:
            CyU3PDebugPrint (8, "CY_U3P_USB_EVENT_CONNECT detected\r\n");
            break;

        case CY_U3P_USB_EVENT_SETCONF:
            /* If the application is already active
             * stop it before re-enabling. */
            if (glIsApplnActive)
            {
                CyFxApplnStop ();
            }

            /* Start the function. */
            CyFxApplnStart ();
            break;

        case CY_U3P_USB_EVENT_RESET:
        case CY_U3P_USB_EVENT_DISCONNECT:
            glForceLinkU2 = CyFalse;

            /* Stop the function. */
            if (glIsApplnActive)
            {
                CyFxApplnStop ();
            }

            if (evtype == CY_U3P_USB_EVENT_DISCONNECT) {
                CyU3PDebugPrint (8, "CY_U3P_USB_EVENT_DISCONNECT detected\r\n");
            }
            break;

        case CY_U3P_USB_EVENT_EP0_STAT_CPLT:
            break;

        default:
            break;
    }
}

/* Callback function to handle LPM requests from the USB 3.0 host. This function is invoked by the API
   whenever a state change from U0 -> U1 or U0 -> U2 happens. If we return CyTrue from this function, the
   FX3 device is retained in the low power state. If we return CyFalse, the FX3 device immediately tries
   to trigger an exit back to U0.

   This application does not have any state in which we should not allow U1/U2 transitions; and therefore
   the function always return CyTrue.
 */
CyBool_t
CyFxApplnLPMRqtCB (
        CyU3PUsbLinkPowerMode link_mode)
{
    return CyTrue;
}

/* Entry function for the glAppThread. */
void
CyFxAppThread_Entry (
        uint32_t input)
{
    CyU3PReturnStatus_t stat;
    CyU3PUsbLinkPowerMode curState;

    /* start running DFC operating mode 0 (UART_ONLY) */
    glCurrentDFCModeIndex = 0;
    glCurrentDFCMode = DFCModes[glCurrentDFCModeIndex];

    glCurrentDFCMode->Init();
    CyU3PDebugPrint (4, "DFC operating mode: %s\r\n", glCurrentDFCMode->name);

    UsbStart();

    while (!glIsApplnActive)
        CyU3PThreadSleep (100);

    for (;;)
    {
        /* Try to get the USB 3.0 link back to U0. */
        if (glForceLinkU2)
        {
            stat = CyU3PUsbGetLinkPowerState (&curState);
            while ((glForceLinkU2) && (stat == CY_U3P_SUCCESS) && (curState == CyU3PUsbLPM_U0))
            {
                /* Repeatedly try to go into U2 state.*/
                CyU3PUsbSetLinkPowerState (CyU3PUsbLPM_U2);
                CyU3PThreadSleep (5);
                stat = CyU3PUsbGetLinkPowerState (&curState);
            }
        }

        if (glSetDFCModeRqt != (uint8_t)-1)
        {
            uint8_t dfcModeIndex = glSetDFCModeRqt;
            glSetDFCModeRqt = -1;
            SetDFCMode(dfcModeIndex);
        }

        if (glStartFx3Rqt)
        {
            glStartFx3Rqt = CyFalse;
            StartFx3();
        }

        if (glStopFx3Rqt)
        {
            glStopFx3Rqt = CyFalse;
            StopFx3();
        }

        if (glStartAdcRqt)
        {
            glStartAdcRqt = CyFalse;
            Si5351(glStartAdcReference, glStartAdcFrequency);
        }

        if (glCurrentDFCMode->LoopActions != NULL)
            glCurrentDFCMode->LoopActions();

        CyU3PThreadSleep (10);
    }
}

/* Application define function which creates the threads. */
void
CyFxApplicationDefine (
        void)
{
    void *ptr = NULL;
    uint32_t ret = CY_U3P_SUCCESS;

    /* Allocate the memory for the threads */
    ptr = CyU3PMemAlloc (CY_FX_GPIFTOUSB_THREAD_STACK);

    /* Create the thread for the application */
    ret = CyU3PThreadCreate (&glAppThread,                      /* App thread structure */
                          "21:Gpit_to_USB",                     /* Thread ID and thread name */
                          CyFxAppThread_Entry,                  /* App thread entry function */
                          0,                                    /* No input parameter to thread */
                          ptr,                                  /* Pointer to the allocated thread stack */
                          CY_FX_GPIFTOUSB_THREAD_STACK,         /* App thread stack size */
                          CY_FX_GPIFTOUSB_THREAD_PRIORITY,      /* App thread priority */
                          CY_FX_GPIFTOUSB_THREAD_PRIORITY,      /* App thread priority */
                          CYU3P_NO_TIME_SLICE,                  /* No time slice for the application thread */
                          CYU3P_AUTO_START                      /* Start the thread immediately */
                          );

    /* Check the return code */
    if (ret != 0)
    {
        /* Thread Creation failed with the error code retThrdCreate */

        /* Add custom recovery or debug actions here */

        /* Application cannot continue */
        /* Loop indefinitely */
        while(1);
    }
}

/*
 * Main function
 */
int
main (void)
{
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    /* Initialize the device */
    CyU3PSysClockConfig_t clockConfig;

    /* setSysClk400 clock configurations */
    clockConfig.setSysClk400  = CyTrue;   /* FX3 device's master clock is set to a frequency > 400 MHz */
    clockConfig.cpuClkDiv     = 2;
    clockConfig.dmaClkDiv     = 2;
    clockConfig.mmioClkDiv    = 2;
    clockConfig.useStandbyClk = CyFalse;
    clockConfig.clkSrc        = CY_U3P_SYS_CLK;
    status = CyU3PDeviceInit (&clockConfig);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* Initialize the caches. The D-Cache is not enabled because it will cause a significant slowing down for
     * an application which does not touch the data in the DMA buffers.
     */
    status = CyU3PDeviceCacheControl (CyTrue, CyFalse, CyFalse);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* This is a non returnable call for initializing the RTOS kernel */
    CyU3PKernelEntry ();

    /* Dummy return to make the compiler happy */
    return 0;

handle_fatal_error:

    /* Cannot recover from this error. */
    while (1);
}


/* internal functions */
static void SetDFCMode(uint8_t dfcModeIndex)
{
    if (dfcModeIndex >= NumDFCModes) {
        CyU3PDebugPrint (4, "Invalid DFC operating mode: %u\r\n", dfcModeIndex);
        return;
    }

    CyU3PDebugPrint (4, "Set new DFC operating mode: %u\r\n", dfcModeIndex);
    CyU3PThreadSleep (10);

    glCurrentDFCMode->DeInit();
    glCurrentDFCModeIndex = dfcModeIndex;
    glCurrentDFCMode = DFCModes[glCurrentDFCModeIndex];
    glCurrentDFCMode->Init();

    CyU3PDebugPrint (4, "DFC operating mode: %s\r\n", glCurrentDFCMode->name);

    CyFxApplnStart ();
}

static void StartFx3()
{
    /* SDDC_FX3 GPIF II state machine checks FW_TRG to start */
    CyU3PGpifControlSWInput(CyTrue);
}


static void StopFx3()
{
    /* SDDC_FX3 GPIF II state machine checks !FW_TRG to stop */
    CyU3PGpifControlSWInput(CyFalse);
}

/* [ ] */

