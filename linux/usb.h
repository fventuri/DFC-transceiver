//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef _STREAMING_CLIENT_USB_H_
#define _STREAMING_CLIENT_USB_H_

#include <libusb-1.0/libusb.h>

typedef struct {
    libusb_device_handle *device_handle;
    libusb_device *device;
    int control_interface;
    int data_interface;
    int data_interface_altsetting;
    int endpoint;
    uint8_t bEndpointAddress;
    uint16_t wMaxPacketSize;
    uint8_t bMaxBurst;
    int packet_size;
} usb_device_t;

int usb_init(usb_device_t *this, const char *firmware_file);
int usb_open(usb_device_t *this, int control_interface, int data_interface, int data_interface_altsetting, int endpoint);
int usb_close(usb_device_t *this);
int usb_control_read(const usb_device_t *this, uint8_t control, uint8_t *data, uint16_t size);
int usb_control_write(const usb_device_t *this, uint8_t control, const uint8_t *data, uint16_t size);

#endif /* _STREAMING_CLIENT_USB_H_ */
