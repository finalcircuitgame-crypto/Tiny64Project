// Simple USB Driver for Tiny64 OS
// Basic UHCI (USB 1.1) support

#ifndef USB_H
#define USB_H

#include <stdint.h>
#include <stdbool.h>

// USB Controller Types
typedef enum {
    USB_CONTROLLER_NONE = 0,
    USB_CONTROLLER_UHCI,    // USB 1.1
    USB_CONTROLLER_OHCI,    // USB 1.1 (companion)
    USB_CONTROLLER_EHCI,    // USB 2.0
    USB_CONTROLLER_XHCI     // USB 3.0
} usb_controller_type_t;

// USB Device States
typedef enum {
    USB_DEVICE_DETACHED = 0,
    USB_DEVICE_ATTACHED,
    USB_DEVICE_POWERED,
    USB_DEVICE_DEFAULT,
    USB_DEVICE_ADDRESS,
    USB_DEVICE_CONFIGURED
} usb_device_state_t;

// USB Request Types
#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09

// USB Descriptor Types
#define USB_DESC_DEVICE           0x01
#define USB_DESC_CONFIGURATION    0x02
#define USB_DESC_STRING           0x03
#define USB_DESC_INTERFACE        0x04
#define USB_DESC_ENDPOINT         0x05

// USB Device Descriptor
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

// USB Configuration Descriptor
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

// USB Interface Descriptor
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

// USB Endpoint Descriptor
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

// USB Setup Packet
typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_packet_t;

// USB Device Structure
typedef struct usb_device {
    usb_device_state_t state;
    uint8_t address;
    usb_device_descriptor_t descriptor;
    struct usb_device* next;
} usb_device_t;

// USB Controller Structure
typedef struct usb_controller {
    usb_controller_type_t type;
    uint32_t base_address;
    uint8_t irq;
    usb_device_t* devices;
    struct usb_controller* next;
} usb_controller_t;

// Function declarations
void usb_init(void);
void usb_scan_controllers(void);
void usb_enumerate_devices(void);
usb_device_t* usb_find_device(uint16_t vendor_id, uint16_t product_id);

// UHCI specific functions
void uhci_init(uint32_t base_addr);
void uhci_reset_controller(void);
void uhci_start_controller(void);
void uhci_stop_controller(void);

// Helper functions
uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

#endif // USB_H
