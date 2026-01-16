// AC97 Audio Driver Implementation for Tiny64 OS
// Basic audio playback support

#include "ac97.h"
#include "../../hal/serial.h" // for serial output
#include "../../include/io.h" // for port I/O
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Forward declaration for kmalloc (defined in kernel)
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);

#define BUFFER_DESCRIPTORS 32  // Number of buffer descriptors

static ac97_device_t* ac97_devices = NULL;

// Read/write operations
static inline uint8_t ac97_read8(ac97_device_t* dev, uint16_t offset) {
    return inb(dev->nabm_base + offset);
}

static inline uint16_t ac97_read16(ac97_device_t* dev, uint16_t offset) {
    return inw(dev->nabm_base + offset);
}

static inline uint32_t ac97_read32(ac97_device_t* dev, uint16_t offset) {
    return inl(dev->nabm_base + offset);
}

static inline void ac97_write8(ac97_device_t* dev, uint16_t offset, uint8_t value) {
    outb(dev->nabm_base + offset, value);
}

static inline void ac97_write16(ac97_device_t* dev, uint16_t offset, uint16_t value) {
    outw(dev->nabm_base + offset, value);
}

static inline void ac97_write32(ac97_device_t* dev, uint16_t offset, uint32_t value) {
    outl(dev->nabm_base + offset, value);
}

void ac97_init(void) {
    serial_write_string("AC97: Initializing audio driver\n");
    ac97_devices = NULL;
}

ac97_device_t* ac97_probe(uint32_t nabm_base, uint32_t mixer_base, uint8_t irq) {
    serial_write_string("AC97: Probing device at NABM 0x");
    // Simple hex print for NABM base
    char hex[5];
    uint32_t val = nabm_base;
    for (int i = 3; i >= 0; i--) {
        hex[i] = "0123456789ABCDEF"[val & 0xF];
        val >>= 4;
    }
    hex[4] = 0;
    serial_write_string(hex);
    serial_write_string(", Mixer 0x");
    val = mixer_base;
    for (int i = 3; i >= 0; i--) {
        hex[i] = "0123456789ABCDEF"[val & 0xF];
        val >>= 4;
    }
    hex[4] = 0;
    serial_write_string(hex);
    serial_write_string("\n");

    // Allocate device structure
    ac97_device_t* dev = (ac97_device_t*)kmalloc(sizeof(ac97_device_t));
    if (!dev) {
        serial_write_string("AC97: Failed to allocate device structure\n");
        return NULL;
    }

    memset(dev, 0, sizeof(ac97_device_t));
    dev->nabm_base = nabm_base;
    dev->mixer_base = mixer_base;
    dev->irq = irq;
    dev->next = ac97_devices;
    ac97_devices = dev;

    // Allocate buffer descriptors
    dev->bd_list = (ac97_buffer_descriptor_t*)kmalloc(sizeof(ac97_buffer_descriptor_t) * BUFFER_DESCRIPTORS);
    if (!dev->bd_list) {
        serial_write_string("AC97: Failed to allocate buffer descriptors\n");
        kfree(dev);
        return NULL;
    }

    memset(dev->bd_list, 0, sizeof(ac97_buffer_descriptor_t) * BUFFER_DESCRIPTORS);

    // Reset the device
    ac97_reset(dev);

    // Set up buffer descriptor list
    ac97_write32(dev, AC97_NABM_PCM_OUT, (uint32_t)dev->bd_list);

    dev->initialized = true;
    serial_write_string("AC97: Device initialized successfully\n");

    return dev;
}

void ac97_reset(ac97_device_t* dev) {
    serial_write_string("AC97: Resetting AC97 codec\n");

    // Reset the codec
    ac97_codec_write(dev, AC97_RESET, 0xFFFF);

    // Wait for reset to complete
    for (volatile int i = 0; i < 10000; i++) {
        // Small delay
    }

    // Set default volumes
    ac97_set_master_volume(dev, 0x08, 0x08); // -12dB
    ac97_set_pcm_volume(dev, 0x08, 0x08);    // -12dB

    serial_write_string("AC97: Reset complete\n");
}

void ac97_set_master_volume(ac97_device_t* dev, uint8_t left, uint8_t right) {
    uint16_t volume = (left << 8) | right;
    ac97_codec_write(dev, AC97_MASTER_VOL, volume);
}

void ac97_set_pcm_volume(ac97_device_t* dev, uint8_t left, uint8_t right) {
    uint16_t volume = (left << 8) | right;
    ac97_codec_write(dev, AC97_PCM_OUT_VOL, volume);
}

int ac97_play_pcm(ac97_device_t* dev, const void* data, uint32_t size, ac97_audio_format_t* format) {
    if (!dev->initialized) {
        serial_write_string("AC97: Device not initialized\n");
        return -1;
    }

    if (size > 65536) { // Limit buffer size
        size = 65536;
    }

    // Set up buffer descriptor
    dev->bd_list[0].buffer_addr = (uint32_t)kmalloc(size);
    if (!dev->bd_list[0].buffer_addr) {
        serial_write_string("AC97: Failed to allocate audio buffer\n");
        return -1;
    }

    // Copy audio data
    memcpy((void*)dev->bd_list[0].buffer_addr, data, size);

    // Set buffer length (in samples)
    dev->bd_list[0].buffer_len = size / (format->bits_per_sample / 8) / format->channels;
    dev->bd_list[0].ioc = 1; // Interrupt on completion
    dev->bd_list[0].bup = 0; // Stop on buffer underrun

    // Set sample rate
    ac97_codec_write(dev, AC97_PCM_FRONT_DAC_RATE, format->sample_rate);

    // Enable PCM output
    uint32_t global_ctl = ac97_read32(dev, AC97_NABM_GLOBAL_CTL);
    global_ctl |= (1 << 0); // Enable PCM out
    ac97_write32(dev, AC97_NABM_GLOBAL_CTL, global_ctl);

    // Start playback
    uint32_t pcm_ctl = ac97_read32(dev, AC97_NABM_PCM_OUT + 4); // Control register
    pcm_ctl |= (1 << 0); // Run
    ac97_write32(dev, AC97_NABM_PCM_OUT + 4, pcm_ctl);

    serial_write_string("AC97: Started PCM playback\n");

    return size;
}

void ac97_stop_playback(ac97_device_t* dev) {
    // Stop PCM output
    uint32_t pcm_ctl = ac97_read32(dev, AC97_NABM_PCM_OUT + 4);
    pcm_ctl &= ~(1 << 0); // Clear run bit
    ac97_write32(dev, AC97_NABM_PCM_OUT + 4, pcm_ctl);

    // Free buffer if allocated
    if (dev->bd_list[0].buffer_addr) {
        kfree((void*)dev->bd_list[0].buffer_addr);
        dev->bd_list[0].buffer_addr = 0;
    }

    serial_write_string("AC97: Stopped PCM playback\n");
}

void ac97_codec_write(ac97_device_t* dev, uint8_t reg, uint16_t value) {
    // Write to mixer register (simplified - assumes immediate write)
    outw(dev->mixer_base + (reg << 1), value);

    // Small delay for codec
    for (volatile int i = 0; i < 100; i++) {}
}

uint16_t ac97_codec_read(ac97_device_t* dev, uint8_t reg) {
    // Read from mixer register (simplified)
    return inw(dev->mixer_base + (reg << 1));
}

void ac97_dump_registers(ac97_device_t* dev) {
    serial_write_string("AC97: Register dump:\n");
    uint16_t master_vol = ac97_codec_read(dev, AC97_MASTER_VOL);
    serial_write_string("Master Volume: 0x");
    char hex[5];
    hex[0] = "0123456789ABCDEF"[master_vol >> 12];
    hex[1] = "0123456789ABCDEF"[(master_vol >> 8) & 0xF];
    hex[2] = "0123456789ABCDEF"[(master_vol >> 4) & 0xF];
    hex[3] = "0123456789ABCDEF"[master_vol & 0xF];
    hex[4] = 0;
    serial_write_string(hex);
    serial_write_string("\n");
}
