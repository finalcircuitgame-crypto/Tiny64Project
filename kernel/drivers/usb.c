// Simple USB Driver Implementation for Tiny64 OS
// Basic UHCI (USB 1.1) support with device enumeration

#include "usb.h"
#include "../../hal/serial.h" // for serial output
#include "../../include/io.h" // for port I/O
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// UHCI Register Offsets
#define UHCI_CMD      0x00  // Command register
#define UHCI_STS      0x02  // Status register
#define UHCI_INTR     0x04  // Interrupt enable
#define UHCI_FRNUM    0x06  // Frame number
#define UHCI_FLBASEADD 0x08 // Frame list base address
#define UHCI_SOFMOD   0x0C  // Start of frame modify
#define UHCI_PORTSC1  0x10  // Port 1 status/control
#define UHCI_PORTSC2  0x12  // Port 2 status/control

// UHCI Command bits
#define UHCI_CMD_RS   (1 << 0)  // Run/Stop
#define UHCI_CMD_HCRESET (1 << 1)  // Host controller reset
#define UHCI_CMD_GRESET  (1 << 2)  // Global reset
#define UHCI_CMD_EGSM    (1 << 3)  // Enter global suspend mode
#define UHCI_CMD_FGR     (1 << 4)  // Force global resume
#define UHCI_CMD_SWDBG   (1 << 5)  // Software debug
#define UHCI_CMD_CF      (1 << 6)  // Configure flag
#define UHCI_CMD_MAXP    (1 << 7)  // Max packet

// UHCI Status bits
#define UHCI_STS_HCHALTED     (1 << 0)
#define UHCI_STS_HCPROCERR    (1 << 1)
#define UHCI_STS_HSERR        (1 << 2)
#define UHCI_STS_RESUMEDETECT (1 << 3)
#define UHCI_STS_USBERRINT    (1 << 4)
#define UHCI_STS_USBINT       (1 << 5)
#define UHCI_STS_TDINT        (1 << 6)

// UHCI Port Status/Control bits
#define UHCI_PORT_CONNECT     (1 << 0)
#define UHCI_PORT_CONNECT_CHG (1 << 1)
#define UHCI_PORT_ENABLE      (1 << 2)
#define UHCI_PORT_ENABLE_CHG  (1 << 3)
#define UHCI_PORT_LINE_STATUS (3 << 4)
#define UHCI_PORT_RESET       (1 << 7)
#define UHCI_PORT_LOW_SPEED   (1 << 8)
#define UHCI_PORT_RESUME      (1 << 10)

static usb_controller_t* usb_controllers = NULL;
static usb_device_t* usb_devices = NULL;

// PCI functions (simplified)
uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_write_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

// UHCI Controller Functions
void uhci_init(uint32_t base_addr) {
    serial_write_string("USB: Initializing UHCI controller at 0x");
    // Simple hex print for base address
    char hex[9];
    uint32_t val = base_addr;
    for (int i = 7; i >= 0; i--) {
        hex[i] = "0123456789ABCDEF"[val & 0xF];
        val >>= 4;
    }
    hex[8] = 0;
    serial_write_string(hex);
    serial_write_string("\n");

    // Reset the controller
    outw(base_addr + UHCI_CMD, UHCI_CMD_HCRESET);
    // Wait for reset to complete
    for (volatile int i = 0; i < 10000; i++) {
        if (!(inw(base_addr + UHCI_CMD) & UHCI_CMD_HCRESET))
            break;
    }

    // Configure the controller
    outw(base_addr + UHCI_INTR, 0); // Disable interrupts
    outw(base_addr + UHCI_FRNUM, 0); // Start at frame 0
    outl(base_addr + UHCI_FLBASEADD, 0); // No frame list yet

    // Clear status
    outw(base_addr + UHCI_STS, 0xFFFF);

    serial_write_string("USB: UHCI controller initialized\n");
}

void uhci_reset_controller(void) {
    // Implementation for resetting UHCI controller
    serial_write_string("USB: UHCI controller reset\n");
}

void uhci_start_controller(void) {
    serial_write_string("USB: Starting UHCI controller\n");
    // Set configure flag and run
    // outw(base_addr + UHCI_CMD, UHCI_CMD_CF | UHCI_CMD_RS);
}

void uhci_stop_controller(void) {
    serial_write_string("USB: Stopping UHCI controller\n");
    // Clear run bit
    // outw(base_addr + UHCI_CMD, UHCI_CMD_CF);
}

// USB Initialization
void usb_init(void) {
    serial_write_string("USB: Initializing USB subsystem\n");
    usb_controllers = NULL;
    usb_devices = NULL;
}

void usb_scan_controllers(void) {
    serial_write_string("USB: Scanning for USB controllers\n");

    // Scan PCI for USB controllers (simplified)
    // In a real implementation, we'd scan all PCI devices
    // For now, just check common UHCI locations

    // Check for UHCI controllers (class 0x0C, subclass 0x03, prog-if 0x00)
    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t config0 = pci_read_config_dword(bus, slot, 0, 0);
            uint32_t config8 = pci_read_config_dword(bus, slot, 0, 8);

            // Check if device exists
            if (config0 == 0xFFFFFFFF)
                continue;

            uint8_t class_code = (config8 >> 24) & 0xFF;
            uint8_t subclass = (config8 >> 16) & 0xFF;
            uint8_t prog_if = (config8 >> 8) & 0xFF;

            if (class_code == 0x0C && subclass == 0x03) { // USB controller
                if (prog_if == 0x00) { // UHCI
                    uint32_t bar0 = pci_read_config_dword(bus, slot, 0, 0x10);
                    uint32_t base_addr = bar0 & 0xFFFFFFF0;

                    serial_write_string("USB: Found UHCI controller at PCI ");
                    // Print bus:slot
                    char loc[8];
                    loc[0] = '0' + (bus / 10);
                    loc[1] = '0' + (bus % 10);
                    loc[2] = ':';
                    loc[3] = '0' + (slot / 10);
                    loc[4] = '0' + (slot % 10);
                    loc[5] = 0;
                    serial_write_string(loc);
                    serial_write_string("\n");

                    // Enable bus mastering and I/O space
                    uint32_t config4 = pci_read_config_dword(bus, slot, 0, 4);
                    config4 |= (1 << 2) | (1 << 0); // Bus master + I/O space
                    pci_write_config_dword(bus, slot, 0, 4, config4);

                    uhci_init(base_addr);

                } else if (prog_if == 0x10) { // OHCI
                    serial_write_string("USB: Found OHCI controller (not supported yet)\n");
                } else if (prog_if == 0x20) { // EHCI
                    serial_write_string("USB: Found EHCI controller (not supported yet)\n");
                }
            }
        }
    }
}

void usb_enumerate_devices(void) {
    serial_write_string("USB: Enumerating USB devices\n");
    // This would be complex - need to set up frame lists, TDs, etc.
    // For now, just report that we're scanning
    serial_write_string("USB: Device enumeration not fully implemented yet\n");
}

usb_device_t* usb_find_device(uint16_t vendor_id, uint16_t product_id) {
    usb_device_t* dev = usb_devices;
    while (dev) {
        if (dev->descriptor.idVendor == vendor_id &&
            dev->descriptor.idProduct == product_id) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}
