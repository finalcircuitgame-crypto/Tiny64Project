// IDE/ATA Storage Driver for Tiny64 OS
// Basic hard disk support

#ifndef IDE_H
#define IDE_H

#include <stdint.h>
#include <stdbool.h>

// IDE Registers (Primary channel)
#define IDE_DATA        0x1F0  // Data register
#define IDE_ERROR       0x1F1  // Error register
#define IDE_FEATURES    0x1F1  // Features register
#define IDE_SECTOR_COUNT 0x1F2 // Sector count
#define IDE_LBA_LOW     0x1F3  // LBA low
#define IDE_LBA_MID     0x1F4  // LBA mid
#define IDE_LBA_HIGH    0x1F5  // LBA high
#define IDE_DEVICE      0x1F6  // Device register
#define IDE_STATUS      0x1F7  // Status register
#define IDE_COMMAND     0x1F7  // Command register

// IDE Control Registers
#define IDE_ALT_STATUS  0x3F6  // Alternate status
#define IDE_DEVICE_CTRL 0x3F6  // Device control
#define IDE_DRIVE_ADDR  0x3F7  // Drive address

// IDE Status bits
#define IDE_STS_ERR     (1 << 0)  // Error
#define IDE_STS_IDX     (1 << 1)  // Index
#define IDE_STS_CORR    (1 << 2)  // Corrected data
#define IDE_STS_DRQ     (1 << 3)  // Data request ready
#define IDE_STS_DSC     (1 << 4)  // Drive seek complete
#define IDE_STS_DWF     (1 << 5)  // Drive write fault
#define IDE_STS_DRDY    (1 << 6)  // Drive ready
#define IDE_STS_BSY     (1 << 7)  // Busy

// IDE Commands
#define IDE_CMD_READ_SECTORS    0x20  // Read sectors with retry
#define IDE_CMD_WRITE_SECTORS   0x30  // Write sectors with retry
#define IDE_CMD_IDENTIFY        0xEC  // Identify drive
#define IDE_CMD_SET_FEATURES     0xEF  // Set features

// IDE Device types
typedef enum {
    IDE_DEVICE_NONE = 0,
    IDE_DEVICE_ATA,
    IDE_DEVICE_ATAPI
} ide_device_type_t;

// IDE Drive structure
typedef struct ide_drive {
    uint16_t base_port;      // Base I/O port
    uint8_t drive_num;       // 0 = master, 1 = slave
    ide_device_type_t type;  // ATA or ATAPI
    bool present;            // Drive exists
    uint32_t sectors;        // Total sectors
    uint16_t cylinders;      // Number of cylinders
    uint16_t heads;          // Number of heads
    uint16_t sectors_per_track; // Sectors per track
    char model[41];          // Model string
    char serial[21];         // Serial number
    struct ide_drive* next;
} ide_drive_t;

// Function declarations
void ide_init(void);
void ide_detect_drives(void);
ide_drive_t* ide_get_drive(uint8_t drive_num);
int ide_read_sectors(ide_drive_t* drive, uint32_t lba, uint8_t count, void* buffer);
int ide_write_sectors(ide_drive_t* drive, uint32_t lba, uint8_t count, const void* buffer);
int ide_identify_drive(ide_drive_t* drive);

// Utility functions
void ide_wait_ready(uint16_t base_port);
void ide_select_drive(ide_drive_t* drive);
uint8_t ide_read_status(uint16_t base_port);
void ide_dump_drive_info(ide_drive_t* drive);

#endif // IDE_H
