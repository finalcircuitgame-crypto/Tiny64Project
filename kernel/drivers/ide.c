// IDE/ATA Storage Driver Implementation for Tiny64 OS
// Basic hard disk read/write support

#include "ide.h"
#include "../../hal/serial.h" // for serial output
#include "../../include/io.h" // for port I/O
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Forward declarations for kernel memory functions
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);

#define IDE_TIMEOUT 5000000  // Timeout for IDE operations

static ide_drive_t* ide_drives[4] = {NULL, NULL, NULL, NULL}; // Primary master/slave, Secondary master/slave

// Read/write operations
static inline uint8_t ide_read8(uint16_t port) {
    return inb(port);
}

static inline uint16_t ide_read16(uint16_t port) {
    return inw(port);
}

static inline void ide_write8(uint16_t port, uint8_t value) {
    outb(port, value);
}

static inline void ide_write16(uint16_t port, uint16_t value) {
    outw(port, value);
}

static inline void ide_read_buffer(uint16_t port, void* buffer, uint32_t count) {
    uint16_t* buf = (uint16_t*)buffer;
    for (uint32_t i = 0; i < count / 2; i++) {
        buf[i] = ide_read16(port);
    }
}

static inline void ide_write_buffer(uint16_t port, const void* buffer, uint32_t count) {
    const uint16_t* buf = (const uint16_t*)buffer;
    for (uint32_t i = 0; i < count / 2; i++) {
        ide_write16(port, buf[i]);
    }
}

void ide_init(void) {
    serial_write_string("IDE: Initializing storage driver\n");

    // Reset all drive pointers
    for (int i = 0; i < 4; i++) {
        ide_drives[i] = NULL;
    }
}

void ide_detect_drives(void) {
    serial_write_string("IDE: Detecting drives\n");

    // Check primary channel
    ide_drive_t* primary_master = (ide_drive_t*)kmalloc(sizeof(ide_drive_t));
    if (primary_master) {
        memset(primary_master, 0, sizeof(ide_drive_t));
        primary_master->base_port = 0x1F0;
        primary_master->drive_num = 0; // Master
        if (ide_identify_drive(primary_master) == 0) {
            ide_drives[0] = primary_master;
            serial_write_string("IDE: Primary master drive detected\n");
        } else {
            kfree(primary_master);
        }
    }

    ide_drive_t* primary_slave = (ide_drive_t*)kmalloc(sizeof(ide_drive_t));
    if (primary_slave) {
        memset(primary_slave, 0, sizeof(ide_drive_t));
        primary_slave->base_port = 0x1F0;
        primary_slave->drive_num = 1; // Slave
        if (ide_identify_drive(primary_slave) == 0) {
            ide_drives[1] = primary_slave;
            serial_write_string("IDE: Primary slave drive detected\n");
        } else {
            kfree(primary_slave);
        }
    }

    // Secondary channel (if supported)
    ide_drive_t* secondary_master = (ide_drive_t*)kmalloc(sizeof(ide_drive_t));
    if (secondary_master) {
        memset(secondary_master, 0, sizeof(ide_drive_t));
        secondary_master->base_port = 0x170;
        secondary_master->drive_num = 0; // Master
        if (ide_identify_drive(secondary_master) == 0) {
            ide_drives[2] = secondary_master;
            serial_write_string("IDE: Secondary master drive detected\n");
        } else {
            kfree(secondary_master);
        }
    }
}

ide_drive_t* ide_get_drive(uint8_t drive_num) {
    if (drive_num < 4) {
        return ide_drives[drive_num];
    }
    return NULL;
}

int ide_identify_drive(ide_drive_t* drive) {
    ide_select_drive(drive);
    ide_wait_ready(drive->base_port);

    // Send IDENTIFY command
    ide_write8(drive->base_port + IDE_COMMAND, IDE_CMD_IDENTIFY);

    // Wait for drive to respond
    uint32_t timeout = IDE_TIMEOUT;
    while (timeout--) {
        uint8_t status = ide_read_status(drive->base_port);
        if (!(status & IDE_STS_BSY)) {
            if (status & IDE_STS_DRQ) {
                // Drive is ready to transfer data
                uint16_t identify_data[256];
                ide_read_buffer(drive->base_port + IDE_DATA, identify_data, 512);

                // Parse identify data
                drive->type = IDE_DEVICE_ATA;
                drive->present = true;

                // Extract model string (words 27-46, little endian)
                for (int i = 0; i < 20; i++) {
                    uint16_t word = identify_data[27 + i];
                    drive->model[i * 2] = word & 0xFF;
                    drive->model[i * 2 + 1] = word >> 8;
                }
                drive->model[40] = 0; // Null terminate

                // Extract serial number (words 10-19)
                for (int i = 0; i < 10; i++) {
                    uint16_t word = identify_data[10 + i];
                    drive->serial[i * 2] = word & 0xFF;
                    drive->serial[i * 2 + 1] = word >> 8;
                }
                drive->serial[20] = 0; // Null terminate

                // Extract geometry
                drive->cylinders = identify_data[1];
                drive->heads = identify_data[3];
                drive->sectors_per_track = identify_data[6];

                // Calculate total sectors (LBA28)
                drive->sectors = (identify_data[61] << 16) | identify_data[60];

                return 0; // Success
            } else if (status & IDE_STS_ERR) {
                // Drive doesn't exist or error
                break;
            }
        }
    }

    return -1; // Failed
}

int ide_read_sectors(ide_drive_t* drive, uint32_t lba, uint8_t count, void* buffer) {
    if (!drive || !drive->present || count == 0) {
        return -1;
    }

    ide_select_drive(drive);
    ide_wait_ready(drive->base_port);

    // Set up LBA addressing
    ide_write8(drive->base_port + IDE_SECTOR_COUNT, count);
    ide_write8(drive->base_port + IDE_LBA_LOW, lba & 0xFF);
    ide_write8(drive->base_port + IDE_LBA_MID, (lba >> 8) & 0xFF);
    ide_write8(drive->base_port + IDE_LBA_HIGH, (lba >> 16) & 0xFF);

    // Device register: LBA mode, drive select
    uint8_t device = 0xE0 | (drive->drive_num << 4) | ((lba >> 24) & 0x0F);
    ide_write8(drive->base_port + IDE_DEVICE, device);

    // Send READ command
    ide_write8(drive->base_port + IDE_COMMAND, IDE_CMD_READ_SECTORS);

    // Read sectors
    for (uint8_t sector = 0; sector < count; sector++) {
        // Wait for data ready
        uint32_t timeout = IDE_TIMEOUT;
        while (timeout--) {
            uint8_t status = ide_read_status(drive->base_port);
            if (status & IDE_STS_ERR) {
                serial_write_string("IDE: Read error\n");
                return -1;
            }
            if (status & IDE_STS_DRQ) {
                break;
            }
        }

        if (timeout == 0) {
            serial_write_string("IDE: Read timeout\n");
            return -1;
        }

        // Read sector data (512 bytes)
        ide_read_buffer(drive->base_port + IDE_DATA, (uint8_t*)buffer + (sector * 512), 512);
    }

    return count;
}

int ide_write_sectors(ide_drive_t* drive, uint32_t lba, uint8_t count, const void* buffer) {
    if (!drive || !drive->present || count == 0) {
        return -1;
    }

    ide_select_drive(drive);
    ide_wait_ready(drive->base_port);

    // Set up LBA addressing
    ide_write8(drive->base_port + IDE_SECTOR_COUNT, count);
    ide_write8(drive->base_port + IDE_LBA_LOW, lba & 0xFF);
    ide_write8(drive->base_port + IDE_LBA_MID, (lba >> 8) & 0xFF);
    ide_write8(drive->base_port + IDE_LBA_HIGH, (lba >> 16) & 0xFF);

    // Device register: LBA mode, drive select
    uint8_t device = 0xE0 | (drive->drive_num << 4) | ((lba >> 24) & 0x0F);
    ide_write8(drive->base_port + IDE_DEVICE, device);

    // Send WRITE command
    ide_write8(drive->base_port + IDE_COMMAND, IDE_CMD_WRITE_SECTORS);

    // Write sectors
    for (uint8_t sector = 0; sector < count; sector++) {
        // Wait for drive ready
        uint32_t timeout = IDE_TIMEOUT;
        while (timeout--) {
            uint8_t status = ide_read_status(drive->base_port);
            if (status & IDE_STS_ERR) {
                serial_write_string("IDE: Write error\n");
                return -1;
            }
            if (status & IDE_STS_DRQ) {
                break;
            }
        }

        if (timeout == 0) {
            serial_write_string("IDE: Write timeout\n");
            return -1;
        }

        // Write sector data (512 bytes)
        ide_write_buffer(drive->base_port + IDE_DATA, (uint8_t*)buffer + (sector * 512), 512);
    }

    // Wait for write to complete
    uint32_t timeout = IDE_TIMEOUT;
    while (timeout--) {
        uint8_t status = ide_read_status(drive->base_port);
        if (!(status & IDE_STS_BSY)) {
            break;
        }
    }

    return count;
}

void ide_wait_ready(uint16_t base_port) {
    uint32_t timeout = IDE_TIMEOUT;
    while (timeout--) {
        uint8_t status = ide_read_status(base_port);
        if (!(status & IDE_STS_BSY) && (status & IDE_STS_DRDY)) {
            break;
        }
    }
}

void ide_select_drive(ide_drive_t* drive) {
    uint8_t device = 0xA0 | (drive->drive_num << 4);
    ide_write8(drive->base_port + IDE_DEVICE, device);

    // Wait 400ns for drive select to take effect
    for (volatile int i = 0; i < 4; i++) {
        ide_read_status(drive->base_port);
    }
}

uint8_t ide_read_status(uint16_t base_port) {
    return ide_read8(base_port + IDE_STATUS);
}

void ide_dump_drive_info(ide_drive_t* drive) {
    if (!drive || !drive->present) {
        serial_write_string("IDE: Drive not present\n");
        return;
    }

    serial_write_string("IDE Drive Info:\n");
    serial_write_string("Model: ");
    serial_write_string(drive->model);
    serial_write_string("\n");

    serial_write_string("Serial: ");
    serial_write_string(drive->serial);
    serial_write_string("\n");

    serial_write_string("Sectors: ");
    // Simple decimal print for sectors
    char num[16];
    uint32_t val = drive->sectors;
    int i = 0;
    if (val == 0) {
        num[i++] = '0';
    } else {
        while (val > 0) {
            num[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    while (i > 0) {
        serial_write_char(num[--i]);
    }
    serial_write_string("\n");
}
