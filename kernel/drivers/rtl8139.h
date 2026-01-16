// RTL8139 Network Driver for Tiny64 OS
// Basic Ethernet support

#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include <stdbool.h>

// RTL8139 Registers
#define RTL8139_MAC0          0x00  // Ethernet hardware address
#define RTL8139_MAR0          0x08  // Multicast filter
#define RTL8139_TXSTATUS0     0x10  // Transmit status (8 registers)
#define RTL8139_TXADDR0       0x20  // Tx descriptors (4 registers)
#define RTL8139_RXBUF         0x30  // Receive buffer start address
#define RTL8139_CHIPCMD       0x37  // Command register
#define RTL8139_RXBUFTAIL     0x38  // Current address of packet read
#define RTL8139_RXBUFHEAD     0x3A  // Current buffer address
#define RTL8139_INTRMASK      0x3C  // Interrupt mask
#define RTL8139_INTRSTATUS    0x3E  // Interrupt status
#define RTL8139_TXCONFIG      0x40  // Tx config
#define RTL8139_RXCONFIG      0x44  // Rx config
#define RTL8139_TIMER         0x48  // A general purpose counter
#define RTL8139_RXMISSED      0x4C  // 24 bits valid, write clears
#define RTL8139_CFG9346       0x50  // 93C46 command register
#define RTL8139_CONFIG0       0x51  // Configuration reg 0
#define RTL8139_CONFIG1       0x52  // Configuration reg 1
#define RTL8139_TIMERINT      0x54  // Timer interrupt register
#define RTL8139_MSR           0x58  // Media status register
#define RTL8139_CONFIG3       0x59  // Configuration reg 3
#define RTL8139_CONFIG4       0x5A  // Configuration reg 4
#define RTL8139_MULINT        0x5C  // Multiple interrupt select
#define RTL8139_RERID         0x5E  // PCI Revision ID
#define RTL8139_TSAD          0x60  // Transmit status of all descriptors
#define RTL8139_BMCR          0x62  // Basic mode control register
#define RTL8139_BMSR          0x64  // Basic mode status register
#define RTL8139_ANAR          0x66  // Auto-negotiation advertisement
#define RTL8139_ANLPAR        0x68  // Auto-negotiation link partner
#define RTL8139_ANER          0x6A  // Auto-negotiation expansion
#define RTL8139_DIS           0x6C  // Disconnect counter
#define RTL8139_FCSC          0x6E  // False carrier sense counter
#define RTL8139_NWAYTR        0x70  // N-way test register
#define RTL8139_REC           0x72  // RX_ER counter
#define RTL8139_CSCR          0x74  // Carrier sense counter
#define RTL8139_PHY1_PARM     0x78  // PHY1 parameter
#define RTL8139_TW_PARM       0x7C  // Twister parameter
#define RTL8139_PHY2_PARM     0x80  // PHY2 parameter

// RTL8139 Command bits
#define RTL8139_CMD_RESET     0x10
#define RTL8139_CMD_RX_ENABLE 0x08
#define RTL8139_CMD_TX_ENABLE 0x04

// RTL8139 Interrupt bits
#define RTL8139_INT_ROK       (1 << 0)   // Receive OK
#define RTL8139_INT_RER       (1 << 1)   // Receive Error
#define RTL8139_INT_TOK       (1 << 2)   // Transmit OK
#define RTL8139_INT_TER       (1 << 3)   // Transmit Error
#define RTL8139_INT_RXOVW     (1 << 4)   // Rx Buffer Overflow
#define RTL8139_INT_PUN       (1 << 5)   // Packet Underrun/Link Change
#define RTL8139_INT_FOVW      (1 << 6)   // Rx FIFO Overflow
#define RTL8139_INT_LENCHG    (1 << 13)  // Cable Length Change

// Ethernet frame structure
typedef struct {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t type;
    uint8_t data[];
} __attribute__((packed)) ethernet_frame_t;

// Network interface structure
typedef struct rtl8139_device {
    uint32_t io_base;
    uint8_t irq;
    uint8_t mac_addr[6];
    uint8_t* rx_buffer;
    uint32_t rx_buffer_size;
    uint32_t current_rx;
    bool link_up;
    struct rtl8139_device* next;
} rtl8139_device_t;

// Function declarations
void rtl8139_init(void);
rtl8139_device_t* rtl8139_probe(uint32_t io_base, uint8_t irq);
void rtl8139_reset(rtl8139_device_t* dev);
void rtl8139_enable_rx_tx(rtl8139_device_t* dev);
void rtl8139_set_rx_config(rtl8139_device_t* dev);
void rtl8139_get_mac_address(rtl8139_device_t* dev);
void rtl8139_init_rx_buffer(rtl8139_device_t* dev);
void rtl8139_enable_interrupts(rtl8139_device_t* dev);

// Packet handling
int rtl8139_send_packet(rtl8139_device_t* dev, const void* data, uint32_t len);
int rtl8139_receive_packet(rtl8139_device_t* dev, void* buffer, uint32_t buffer_size);

// Utility functions
void rtl8139_dump_registers(rtl8139_device_t* dev);

#endif // RTL8139_H
