//
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "usb.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static const uint16_t fx3_streamer_example[] = { 0x04b4, 0x00f1 };
static const uint16_t fx3_dfu_mode[]         = { 0x04b4, 0x00f3 };

static const unsigned int timeout = 5000;  /* timeout (in ms) for each command */

static int upload_fx3_firmware(const char *firmware_file, libusb_device_handle *device_handle);


int usb_init(usb_device_t *this, const char *firmware_file)
{
    int status;

    status = libusb_init(NULL);
    if (status != LIBUSB_SUCCESS) {
        fprintf(stderr, "usb_init - error in libusb_init(): %s\n", libusb_strerror(status));
        return -1;
    }

    /* look for streamer device first; if found return that */
    libusb_device_handle *device_handle;
    device_handle = libusb_open_device_with_vid_pid(NULL, fx3_streamer_example[0], fx3_streamer_example[1]);
    if (device_handle != NULL) {
        this->device_handle = device_handle;
        return 0;
    }

    fprintf(stderr, "FX3 streamer example not found - trying FX3 in DFU mode\n");

    device_handle = libusb_open_device_with_vid_pid(NULL, fx3_dfu_mode[0], fx3_dfu_mode[1]);
    if (device_handle == NULL) {
        fprintf(stderr, "usb_init - FX3 in DFU mode not found\n");
        return -1;
    }

    fprintf(stderr, "upload FX3 firmware\n");

    if (upload_fx3_firmware(firmware_file, device_handle) != 0) {
        fprintf(stderr, "usb_init - FX3 firmware upload failed\n");
        libusb_close(device_handle);
        return -1;
    }

    /* look again for streamer device */
    for (int retry = 0; retry < 10; retry++) {
        device_handle = libusb_open_device_with_vid_pid(NULL, fx3_streamer_example[0], fx3_streamer_example[1]);
        if (device_handle != NULL) {
            fprintf(stderr, "FX3 firmware upload OK (retry=%d)\n", retry);
            break;
        }
        usleep(100000); /* wait 100ms before checking again */
    }

    if (device_handle == NULL) {
        fprintf(stderr, "usb_init - FX3 firmware upload failed - FX3 streamer example not found\n");
        libusb_close(device_handle);
        return -1;
    }

    this->device_handle = device_handle;
    return 0;
}

int usb_open(usb_device_t *this, int control_interface, int data_interface, int data_interface_altsetting, int endpoint)
{
    int status;

    this->device = libusb_get_device(this->device_handle);
    if (this->device == NULL) {
        fprintf(stderr, "usb_open - libusb_get_device() failed\n");
        libusb_close(this->device_handle);
        return -1;
    }

    this->control_interface = control_interface;

    status = libusb_kernel_driver_active(this->device_handle, this->control_interface);
    if (status != 0) {
        if (status == 1) {
            fprintf(stderr, "usb_open - a kernel driver is currently active on the control interface\n");
        } else {
            fprintf(stderr, "usb_open - error in libusb_kernel_driver_active(): %s\n", libusb_strerror(status));
        }
        libusb_close(this->device_handle);
        return -1;
    }

    status = libusb_claim_interface(this->device_handle, this->control_interface);
    if (status != LIBUSB_SUCCESS) {
        fprintf(stderr, "usb_open - error in libusb_claim_interface(): %s\n", libusb_strerror(status));
        libusb_close(this->device_handle);
        return -1;
    }

    struct libusb_config_descriptor *config;
    status = libusb_get_active_config_descriptor(this->device, &config);
    if (status != LIBUSB_SUCCESS) {
        fprintf(stderr, "usb_open - error in libusb_get_active_config_descriptor(): %s\n", libusb_strerror(status));
        libusb_close(this->device_handle);
        return -1;
    }

    /* 1 - data interface */
    if (data_interface >= config->bNumInterfaces) {
        fprintf(stderr, "usb_open - invalid data interface number: valid range is [0-%d]\n", config->bNumInterfaces - 1);
        libusb_free_config_descriptor(config);
        libusb_close(this->device_handle);
        return -1;

    }
    this->data_interface = data_interface;
    const struct libusb_interface *interface = &config->interface[data_interface];

    /* 2 - data interface alternate setting (normally 0) */
    if (data_interface_altsetting >= interface->num_altsetting) {
        fprintf(stderr, "usb_open - invalid data interface alternate setting number: valid range is [0-%d]\n", interface->num_altsetting - 1);
        libusb_free_config_descriptor(config);
        libusb_close(this->device_handle);
        return -1;

    }
    this->data_interface_altsetting = data_interface_altsetting;
    const struct libusb_interface_descriptor *interface_descriptor = &interface->altsetting[data_interface_altsetting];

    /* 3 - data endpoint */
    if (endpoint >= interface_descriptor->bNumEndpoints) {
        fprintf(stderr, "usb_open - invalid data endpoint number: valid range is [0-%d]\n", interface_descriptor->bNumEndpoints - 1);
        libusb_free_config_descriptor(config);
        libusb_close(this->device_handle);
        return -1;

    }
    this->endpoint = endpoint;
    const struct libusb_endpoint_descriptor *endpoint_descriptor = &interface_descriptor->endpoint[endpoint];
    this->bEndpointAddress = endpoint_descriptor->bEndpointAddress;
    this->wMaxPacketSize = endpoint_descriptor->wMaxPacketSize;

    struct libusb_ss_endpoint_companion_descriptor *ep_comp ;
    status = libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint_descriptor, &ep_comp);
    if (status != LIBUSB_SUCCESS) {
        fprintf(stderr, "usb_open - error in libusb_get_ss_endpoint_companion_descriptor(): %s\n", libusb_strerror(status));
        fprintf(stderr, "*** make sure it is running in USB 3.0 SuperSpeed mode ***\n");
        libusb_free_config_descriptor(config);
        libusb_close(this->device_handle);
        return -1;
    }
    this->bMaxBurst = ep_comp->bMaxBurst;

    libusb_free_config_descriptor(config);

    fprintf(stderr, "endpoint address: 0x%02hhx\n", this->bEndpointAddress);
    //fprintf(stderr, "max packet size: %hu\n", this->wMaxPacketSize);
    //fprintf(stderr, "SS max burst: %hhu\n", this->bMaxBurst);

    this->packet_size = this->wMaxPacketSize * (this->bMaxBurst + 1);

    return 0;
}

int usb_close(usb_device_t *this)
{
    if (this->device_handle != NULL) {
        libusb_release_interface(this->device_handle, this->control_interface);
        libusb_close(this->device_handle);
    }
    libusb_exit(NULL);
    return 0;
}

int usb_control_read(const usb_device_t *this, uint8_t control, uint8_t *data, uint16_t size)
{
   assert(data != NULL);
   assert(size > 0);

   int status;
   status = libusb_control_transfer(this->device_handle,
                                    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR,
                                    control,
                                    0,
                                    0,
                                    data,
                                    size,
                                    timeout);
    if (status < 0) {
        fprintf(stderr, "usb_control_read - error in libusb_control_transfer(%hhu): %s\n", control, libusb_strerror(status));
        return -1; 
    }

    return 0;
}

int usb_control_write(const usb_device_t *this, uint8_t control, const uint8_t *data, uint16_t size)
{
   int status;
   status = libusb_control_transfer(this->device_handle,
                                    LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                    control,
                                    0,
                                    0,
                                    (uint8_t *)data,
                                    size,
                                    timeout);
    if (status < 0) {
        fprintf(stderr, "usb_control_write - error in libusb_control_transfer(%hhu): %s\n", control, libusb_strerror(status));
        return -1; 
    }

    return 0;
}


/* internal functions */
static int upload_fx3_firmware(const char *firmware_file, libusb_device_handle *device_handle)
{
    const uint8_t bmRequestTypeRead = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    const uint8_t bmRequestTypeWrite = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
    const uint8_t bRequest = 0xa0;            /* vendor command: RW_INTERNAL */
    const size_t max_write_size = 4 * 1024;   /* max write size in bytes */

    /* read bootloader version */
    uint32_t bootloader_version_address = 0xffff0020;
    uint32_t bootloader_version;
    int status = libusb_control_transfer(device_handle,
                                         bmRequestTypeRead,
                                         bRequest,
                                         bootloader_version_address & 0xffff,
                                         bootloader_version_address >> 16,
                                         (uint8_t *)&bootloader_version,
                                         (uint16_t)sizeof(bootloader_version),
                                         timeout);
    if (status < 0) {
        fprintf(stderr, "error in libusb_control_transfer(): %s\n", libusb_strerror(status));
        return -1;
    }
    fprintf(stderr, "FX3 bootloader version: 0x%08x\n", bootloader_version);

    int fd = open(firmware_file, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open(%s) failed: %s\n", firmware_file, strerror(errno));
        return -1;
    }
    struct stat firmware_stat;
    if (fstat(fd, &firmware_stat) == -1) {
        fprintf(stderr, "stat(%s) failed: %s\n", firmware_file, strerror(errno));
        close(fd);
        return -1;
    }
    size_t filesize = firmware_stat.st_size;
    uint8_t *firmware_image = malloc(filesize);

    /* read the complete firmware image in memory */
    ssize_t nread = read(fd, firmware_image, filesize);
    close(fd);
    if (nread != (ssize_t)filesize) {
        fprintf(stderr, "read full image failed - nread=%ld\n", nread);
        free(firmware_image);
        return -1;
    }
 
    /* the first two bytes of the image should always be 'CY' */
    if (nread < 2 || strncmp((const char *)&firmware_image[0], "CY", 2) != 0) {
        fprintf(stderr, "no \"CY\" header prefix in firmware image\n");
        free(firmware_image);
        return -1;
    } 

    /* 3rd byte: bImageCTL */
    if (nread < 3 || firmware_image[2] & 0x01) {
        fprintf(stderr, "firmware image does not contain executable code\n");
        free(firmware_image);
        return -1;
    }

    /* 4th byte: bImageType */
    if (nread < 4 || firmware_image[3] != 0xb0) {
        fprintf(stderr, "firmware image type is not FW w/ checksum\n");
        free(firmware_image);
        return -1;
    }

    /* upload the firmware image to FX3 RAM */
    uint32_t checksum = 0;
    for (size_t index = 4; index < filesize; ) {
        uint32_t *data = (uint32_t *)(firmware_image + index);
        uint32_t length = data[0];
        uint32_t address = data[1];
        index += sizeof(data[0]) + sizeof(data[1]);
        uint32_t block_length = length * sizeof(uint32_t);
        if (length != 0) {
            for (int i = 2; i < (int)length + 2; i++) {
                checksum += data[i];
            }
            uint8_t *block_start = firmware_image + index;
            uint8_t *block_end = block_start + block_length;
            while (block_start < block_end) {
                int chunk_length = block_end - block_start;
                if (chunk_length > (int)max_write_size) {
                    chunk_length = max_write_size;
                }
                int status = libusb_control_transfer(device_handle,
                                                     bmRequestTypeWrite,
                                                     bRequest,
                                                     address & 0xffff,
                                                     address >> 16,
                                                     block_start,
                                                     (uint16_t)chunk_length,
                                                     timeout);
                if (status < 0) {
                    fprintf(stderr, "error in libusb_control_transfer(): %s\n", libusb_strerror(status));
                    free(firmware_image);
                    return -1;
                } else if (status == 0) {
                    fprintf(stderr, "error in libusb_control_transfer() - 0 bytes transferred\n");
                    free(firmware_image);
                    return -1;
                }
                // libusb_control_transfer returns the number of bytes actually transferred
                address += status;
                block_start += status;
            }
        } else {
            if (checksum != data[2]) {
                fprintf(stderr, "checksum error in firmware image - actual=0x%08x expecting=0x%08x\n", checksum, data[2]);
                free(firmware_image);
                return -1;
            }
            usleep(100000);  /* wait 100ms before transferring execution */
            uint32_t program_entry = address;
            fprintf(stderr, "transfer execution to Program Entry at 0x%08x\n", program_entry);
            int status = libusb_control_transfer(device_handle,
                                                 bmRequestTypeWrite,
                                                 bRequest,
                                                 program_entry & 0xffff,
                                                 program_entry >> 16,
                                                 NULL,
                                                 0,
                                                 timeout);
            if (status != LIBUSB_SUCCESS) {
                fprintf(stderr, "error in libusb_control_transfer(): %s\n", libusb_strerror(status));
                free(firmware_image);
                return -1;
            }
            break;
        }
        index += block_length;
    }

    free(firmware_image);
    return 0;
}
