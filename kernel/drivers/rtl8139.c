// RTL8139 Network Driver Implementation for Tiny64 OS
// Basic Ethernet support with packet send/receive

#include "rtl8139.h"
#include "../../hal/serial.h" // for serial output
#include "../../include/io.h" // for port I/O
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Forward declaration for kmalloc (defined in kernel)
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);

#define RX_BUFFER_SIZE 8192 + 16 + 1500  // 8K + header + MTU
#define TX_BUFFER_SIZE 1792              // 1792 bytes per Tx descriptor

static rtl8139_device_t* rtl8139_devices = NULL;

// Read/write operations
static inline uint8_t rtl8139_read8(rtl8139_device_t* dev, uint16_t offset) {
    return inb(dev->io_base + offset);
}

static inline uint16_t rtl8139_read16(rtl8139_device_t* dev, uint16_t offset) {
    return inw(dev->io_base + offset);
}

static inline uint32_t rtl8139_read32(rtl8139_device_t* dev, uint16_t offset) {
    return inl(dev->io_base + offset);
}

static inline void rtl8139_write8(rtl8139_device_t* dev, uint16_t offset, uint8_t value) {
    outb(dev->io_base + offset, value);
}

static inline void rtl8139_write16(rtl8139_device_t* dev, uint16_t offset, uint16_t value) {
    outw(dev->io_base + offset, value);
}

static inline void rtl8139_write32(rtl8139_device_t* dev, uint16_t offset, uint32_t value) {
    outl(dev->io_base + offset, value);
}

void rtl8139_init(void) {
    serial_write_string("RTL8139: Initializing network driver\n");
    rtl8139_devices = NULL;
}

rtl8139_device_t* rtl8139_probe(uint32_t io_base, uint8_t irq) {
    serial_write_string("RTL8139: Probing device at IO 0x");
    // Simple hex print for IO base
    char hex[5];
    uint32_t val = io_base;
    for (int i = 3; i >= 0; i--) {
        hex[i] = "0123456789ABCDEF"[val & 0xF];
        val >>= 4;
    }
    hex[4] = 0;
    serial_write_string(hex);
    serial_write_string("\n");

    // Allocate device structure
    rtl8139_device_t* dev = (rtl8139_device_t*)kmalloc(sizeof(rtl8139_device_t));
    if (!dev) {
        serial_write_string("RTL8139: Failed to allocate device structure\n");
        return NULL;
    }

    memset(dev, 0, sizeof(rtl8139_device_t));
    dev->io_base = io_base;
    dev->irq = irq;
    dev->next = rtl8139_devices;
    rtl8139_devices = dev;

    // Reset the device
    rtl8139_reset(dev);

    // Get MAC address
    rtl8139_get_mac_address(dev);

    // Initialize RX buffer
    rtl8139_init_rx_buffer(dev);

    // Set RX configuration
    rtl8139_set_rx_config(dev);

    // Enable RX and TX
    rtl8139_enable_rx_tx(dev);

    // Enable interrupts
    rtl8139_enable_interrupts(dev);

    serial_write_string("RTL8139: Device initialized successfully\n");
    serial_write_string("RTL8139: MAC Address: ");
    for (int i = 0; i < 6; i++) {
        char mac_hex[3];
        mac_hex[0] = "0123456789ABCDEF"[dev->mac_addr[i] >> 4];
        mac_hex[1] = "0123456789ABCDEF"[dev->mac_addr[i] & 0xF];
        mac_hex[2] = 0;
        serial_write_string(mac_hex);
        if (i < 5) serial_write_string(":");
    }
    serial_write_string("\n");

    return dev;
}

void rtl8139_reset(rtl8139_device_t* dev) {
    serial_write_string("RTL8139: Resetting device\n");

    // Send reset command
    rtl8139_write8(dev, RTL8139_CHIPCMD, RTL8139_CMD_RESET);

    // Wait for reset to complete (takes about 10ms)
    for (volatile int i = 0; i < 100000; i++) {
        if (!(rtl8139_read8(dev, RTL8139_CHIPCMD) & RTL8139_CMD_RESET))
            break;
    }

    serial_write_string("RTL8139: Reset complete\n");
}

void rtl8139_enable_rx_tx(rtl8139_device_t* dev) {
    uint8_t cmd = rtl8139_read8(dev, RTL8139_CHIPCMD);
    cmd |= (RTL8139_CMD_RX_ENABLE | RTL8139_CMD_TX_ENABLE);
    rtl8139_write8(dev, RTL8139_CHIPCMD, cmd);
    serial_write_string("RTL8139: RX and TX enabled\n");
}

void rtl8139_set_rx_config(rtl8139_device_t* dev) {
    // Accept broadcast, multicast, and physical match packets
    // Also accept runt packets and CRC errors for promiscuous mode
    uint32_t config = (1 << 0) |  // Accept all packets with destination address
                     (1 << 1) |  // Accept physical match packets
                     (1 << 2) |  // Accept multicast packets
                     (1 << 3) |  // Accept broadcast packets
                     (1 << 4) |  // Accept runt packets
                     (1 << 5);   // Accept CRC error packets

    rtl8139_write32(dev, RTL8139_RXCONFIG, config);
    serial_write_string("RTL8139: RX configuration set\n");
}

void rtl8139_get_mac_address(rtl8139_device_t* dev) {
    for (int i = 0; i < 6; i++) {
        dev->mac_addr[i] = rtl8139_read8(dev, RTL8139_MAC0 + i);
    }
}

void rtl8139_init_rx_buffer(rtl8139_device_t* dev) {
    dev->rx_buffer_size = RX_BUFFER_SIZE;
    dev->rx_buffer = (uint8_t*)kmalloc(dev->rx_buffer_size);

    if (!dev->rx_buffer) {
        serial_write_string("RTL8139: Failed to allocate RX buffer\n");
        return;
    }

    // Set RX buffer address
    rtl8139_write32(dev, RTL8139_RXBUF, (uint32_t)dev->rx_buffer);

    // Set RX buffer size (8K + 16 + 1500)
    rtl8139_write32(dev, RTL8139_RXBUFTAIL, dev->rx_buffer_size);

    dev->current_rx = 0;
    serial_write_string("RTL8139: RX buffer initialized\n");
}

void rtl8139_enable_interrupts(rtl8139_device_t* dev) {
    // Enable interrupts for receive OK, transmit OK, and errors
    uint16_t mask = RTL8139_INT_ROK | RTL8139_INT_TOK | RTL8139_INT_RER | RTL8139_INT_TER;
    rtl8139_write16(dev, RTL8139_INTRMASK, mask);

    // Clear any pending interrupts
    rtl8139_write16(dev, RTL8139_INTRSTATUS, 0xFFFF);

    serial_write_string("RTL8139: Interrupts enabled\n");
}

int rtl8139_send_packet(rtl8139_device_t* dev, const void* data, uint32_t len) {
    if (len > 1500) {
        serial_write_string("RTL8139: Packet too large\n");
        return -1;
    }

    // For simplicity, just use TX descriptor 0
    uint32_t tx_addr = dev->io_base + RTL8139_TXADDR0;

    // Copy packet data to a buffer (we'd normally DMA this)
    uint8_t* tx_buffer = (uint8_t*)kmalloc(len);
    if (!tx_buffer) {
        serial_write_string("RTL8139: Failed to allocate TX buffer\n");
        return -1;
    }

    memcpy(tx_buffer, data, len);

    // Set TX address
    rtl8139_write32(dev, RTL8139_TXADDR0, (uint32_t)tx_buffer);

    // Set TX status (size + ownership)
    rtl8139_write32(dev, RTL8139_TXSTATUS0, len);

    // Wait for transmission to complete (simplified)
    for (volatile int i = 0; i < 10000; i++) {
        uint32_t status = rtl8139_read32(dev, RTL8139_TXSTATUS0);
        if (status & (1 << 15)) { // TOK bit set
            break;
        }
    }

    kfree(tx_buffer);
    return len;
}

int rtl8139_receive_packet(rtl8139_device_t* dev, void* buffer, uint32_t buffer_size) {
    uint16_t rx_head = rtl8139_read16(dev, RTL8139_RXBUFHEAD);
    uint16_t rx_tail = rtl8139_read16(dev, RTL8139_RXBUFTAIL);

    if (rx_head == rx_tail) {
        return 0; // No packets
    }

    // Read packet header
    uint16_t* packet_header = (uint16_t*)(dev->rx_buffer + dev->current_rx);
    uint16_t status = packet_header[0];
    uint16_t length = packet_header[1];

    if (!(status & 0x0001)) { // Check if packet is valid
        return -1;
    }

    if (length > buffer_size) {
        serial_write_string("RTL8139: Packet too large for buffer\n");
        return -1;
    }

    // Copy packet data
    uint8_t* packet_data = (uint8_t*)(packet_header + 2);
    memcpy(buffer, packet_data, length);

    // Update RX buffer position
    dev->current_rx = (dev->current_rx + length + 4 + 3) & ~3; // 4-byte header + 4-byte alignment

    // Update tail pointer
    rtl8139_write16(dev, RTL8139_RXBUFTAIL, dev->current_rx - 16);

    return length;
}

void rtl8139_dump_registers(rtl8139_device_t* dev) {
    serial_write_string("RTL8139: Register dump:\n");
    serial_write_string("CHIPCMD: 0x");
    char hex[3];
    uint8_t val = rtl8139_read8(dev, RTL8139_CHIPCMD);
    hex[0] = "0123456789ABCDEF"[val >> 4];
    hex[1] = "0123456789ABCDEF"[val & 0xF];
    hex[2] = 0;
    serial_write_string(hex);
    serial_write_string("\n");
}
