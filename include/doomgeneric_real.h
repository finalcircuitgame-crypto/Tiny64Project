#ifndef DOOM_GENERIC_REAL
#define DOOM_GENERIC_REAL

#include <stdint.h>

// Kernel-compatible DoomGeneric defines
#ifndef DOOMGENERIC_RESX
#define DOOMGENERIC_RESX 640
#endif

#ifndef DOOMGENERIC_RESY
#define DOOMGENERIC_RESY 400
#endif

typedef uint32_t pixel_t;
extern pixel_t* DG_ScreenBuffer;

// Platform functions that Doom calls
void DG_Init();
void DG_DrawFrame();
void DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs();
int DG_GetKey(int* pressed, unsigned char* key);
void DG_SetWindowTitle(const char * title);

// Doom functions that platform calls
void doomgeneric_Create(int argc, char **argv);
void doomgeneric_Tick();
void doomgeneric_SetBootInfo(BootInfo* info);

#endif
