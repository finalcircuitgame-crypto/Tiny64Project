// AC97 Audio Driver for Tiny64 OS
// Basic audio playback support

#ifndef AC97_H
#define AC97_H

#include <stdint.h>
#include <stdbool.h>

// AC97 Registers (ICH/PIIX4)
#define AC97_NABM_PCM_OUT    0x00  // PCM Out Buffer Descriptor Base Address
#define AC97_NABM_PCM_IN     0x08  // PCM In Buffer Descriptor Base Address
#define AC97_NABM_MIC_IN     0x0C  // Microphone Buffer Descriptor Base Address
#define AC97_NABM_GLOBAL_CTL 0x2C  // Global Control
#define AC97_NABM_GLOBAL_STS 0x30  // Global Status

// AC97 Codec Registers (via mixer)
#define AC97_RESET           0x00  // Reset register
#define AC97_MASTER_VOL      0x02  // Master volume
#define AC97_AUX_OUT_VOL     0x04  // Aux out volume
#define AC97_MONO_VOL        0x06  // Mono volume
#define AC97_MASTER_TONE     0x08  // Master tone (R&L)
#define AC97_PC_BEEP_VOL     0x0A  // PC beep volume
#define AC97_PHONE_VOL       0x0C  // Phone volume
#define AC97_MIC_VOL         0x0E  // MIC volume
#define AC97_LINE_IN_VOL     0x10  // Line in volume
#define AC97_CD_VOL          0x12  // CD volume
#define AC97_VIDEO_VOL       0x14  // Video volume
#define AC97_AUX_IN_VOL      0x16  // Aux in volume
#define AC97_PCM_OUT_VOL     0x18  // PCM out volume
#define AC97_REC_SELECT      0x1A  // Record select
#define AC97_REC_GAIN        0x1C  // Record gain
#define AC97_REC_GAIN_MIC    0x1E  // Record gain MIC
#define AC97_GEN_PURPOSE     0x20  // General purpose
#define AC97_3D_CONTROL      0x22  // 3D control
#define AC97_INT_PAGING      0x24  // INT/Paging register
#define AC97_POWERDOWN       0x26  // Powerdown control/stat
#define AC97_EXT_AUDIO_ID    0x28  // Extended audio ID
#define AC97_EXT_AUDIO_STS   0x2A  // Extended audio status/control
#define AC97_PCM_FRONT_DAC_RATE 0x2C  // PCM front DAC rate
#define AC97_PCM_SURR_DAC_RATE  0x2E  // PCM surround DAC rate
#define AC97_PCM_LFE_DAC_RATE   0x30  // PCM LFE DAC rate
#define AC97_PCM_LR_ADC_RATE    0x32  // PCM LR ADC rate
#define AC97_PCM_MIC_ADC_RATE   0x34  // PCM MIC ADC rate

// Audio buffer descriptor
typedef struct {
    uint32_t buffer_addr;    // Physical address of buffer
    uint16_t buffer_len;     // Length of buffer in samples
    uint16_t reserved:14;
    uint16_t bup:1;          // Buffer underrun policy
    uint16_t ioc:1;          // Interrupt on completion
} __attribute__((packed)) ac97_buffer_descriptor_t;

// AC97 device structure
typedef struct ac97_device {
    uint32_t nabm_base;      // Native Audio Bus Master base
    uint32_t mixer_base;     // Mixer base
    uint8_t irq;
    uint8_t* pcm_buffer;
    uint32_t buffer_size;
    ac97_buffer_descriptor_t* bd_list;
    bool initialized;
    struct ac97_device* next;
} ac97_device_t;

// Audio format
typedef struct {
    uint16_t channels;       // 1 = mono, 2 = stereo
    uint32_t sample_rate;    // Samples per second
    uint16_t bits_per_sample; // 8 or 16
} ac97_audio_format_t;

// Function declarations
void ac97_init(void);
ac97_device_t* ac97_probe(uint32_t nabm_base, uint32_t mixer_base, uint8_t irq);
void ac97_reset(ac97_device_t* dev);
void ac97_set_master_volume(ac97_device_t* dev, uint8_t left, uint8_t right);
void ac97_set_pcm_volume(ac97_device_t* dev, uint8_t left, uint8_t right);
int ac97_play_pcm(ac97_device_t* dev, const void* data, uint32_t size, ac97_audio_format_t* format);
void ac97_stop_playback(ac97_device_t* dev);

// Utility functions
void ac97_codec_write(ac97_device_t* dev, uint8_t reg, uint16_t value);
uint16_t ac97_codec_read(ac97_device_t* dev, uint8_t reg);
void ac97_dump_registers(ac97_device_t* dev);

#endif // AC97_H
