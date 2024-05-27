//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef _INCLUDED_APP_H_
#define _INCLUDED_APP_H_

#include "cyu3types.h"
#include "cyu3usb.h"
#include "cyu3usbconst.h"
#include "cyu3externcstart.h"

#define DEVICE_RELEASE_NUMBER              0x01,0x00            /* USB device release number (bcdDevice) minor,major */

#define CY_FX_GPIFTOUSB_DMA_TX_SIZE        (0)                  /* DMA transfer size is set to infinite */
#define CY_FX_GPIFTOUSB_THREAD_STACK       (0x1000)             /* Bulk loop application thread stack size */
#define CY_FX_GPIFTOUSB_THREAD_PRIORITY    (8)                  /* Bulk loop application thread priority */
#define CY_FX_GPIFTOUSB_PATTERN            (0xAA)               /* 8-bit pattern to be loaded to the source buffers. */

/* Endpoint and socket definitions for the bulk source sink application */

/* To change the producer and consumer EP enter the appropriate EP numbers for the #defines.
 * In the case of IN endpoints enter EP number along with the direction bit.
 * For eg. EP 6 IN endpoint is 0x86
 *     and EP 6 OUT endpoint is 0x06.
 * To change sockets mention the appropriate socket number in the #defines. */

/* Note: For USB 2.0 the endpoints and corresponding sockets are one-to-one mapped
         i.e. EP 1 is mapped to UIB socket 1 and EP 2 to socket 2 so on */

/* consumer endpoint (RX) */
#define CY_FX_EP_CONSUMER               0x81    /* EP 1 IN */
#define CY_FX_EP_CONSUMER_SOCKET        CY_U3P_UIB_SOCKET_CONS_1    /* Socket 1 is consumer */
#define CY_FX_GPIF_PRODUCER_SOCKET_0    CY_U3P_PIB_SOCKET_0
#define CY_FX_GPIF_PRODUCER_SOCKET_1    CY_U3P_PIB_SOCKET_1

/* producer endpoint (TX) */
#define CY_FX_EP_PRODUCER               0x01    /* EP 1 OUT */
#define CY_FX_EP_PRODUCER_SOCKET        CY_U3P_UIB_SOCKET_PROD_1    /* Socket 1 is producer */
#define CY_FX_GPIF_CONSUMER_SOCKET_0    CY_U3P_PIB_SOCKET_0
#define CY_FX_GPIF_CONSUMER_SOCKET_1    CY_U3P_PIB_SOCKET_1
// fv - will use the values below for  RX/TX full-duplex
// (with a new state machine)
//#define CY_FX_GPIF_CONSUMER_SOCKET_0    CY_U3P_PIB_SOCKET_2
//#define CY_FX_GPIF_CONSUMER_SOCKET_1    CY_U3P_PIB_SOCKET_3

/* Burst mode definitions: Only for super speed operation. The maximum burst mode 
 * supported is limited by the USB hosts available. The maximum value for this is 16
 * and the minimum (no-burst) is 1. */

/* Burst length in 1 KB packets. Only applicable to USB 3.0. */
#ifndef CY_FX_EP_BURST_LENGTH
#define CY_FX_EP_BURST_LENGTH           (16)
#endif

/* Size of DMA buffers used by the application. */
#ifndef CY_FX_DMA_BUF_SIZE
#define CY_FX_DMA_BUF_SIZE              (16384)
#endif

/* Number of DMA buffers to be used on the channel. */
#ifndef CY_FX_DMA_BUF_COUNT
#define CY_FX_DMA_BUF_COUNT             (4)
#endif

/* shared functions */
void CyFxAppErrorHandler(CyU3PReturnStatus_t apiRetStatus);
CyBool_t CyFxApplnUSBSetupCB(uint32_t setupdat0, uint32_t setupdat1);
void CyFxApplnUSBEventCB(CyU3PUsbEventType_t evtype, uint16_t evdata);
CyBool_t CyFxApplnLPMRqtCB(CyU3PUsbLinkPowerMode link_mode);

/* Extern definitions for the USB Descriptors */
extern const uint8_t CyFxUSB20DeviceDscr[];
extern const uint8_t CyFxUSB30DeviceDscr[];
extern const uint8_t CyFxUSBDeviceQualDscr[];
extern const uint8_t CyFxUSBFSConfigDscr[];
extern const uint8_t CyFxUSBHSConfigDscr[];
extern const uint8_t CyFxUSBBOSDscr[];
extern const uint8_t CyFxUSBSSConfigDscr[];
extern const uint8_t CyFxUSBStringLangIDDscr[];
extern const uint8_t CyFxUSBManufactureDscr[];
extern const uint8_t CyFxUSBProductDscr[];
extern uint8_t CyFxUSBSerialNumberDscr[];
extern const uint8_t CyFxUsbOSDscr[];

#include <cyu3externcend.h>

#endif /* _INCLUDED_APP_H_ */

/*[]*/
