#include <stdio.h>

#include "m_argv.h"

#include "doomgeneric.h"
#include "../include/kernel.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);

// Track if Doom has been initialized
static int doom_initialized = 0;

void doomgeneric_Create(int argc, char **argv)
{
	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

	DG_ScreenBuffer = kmalloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

	DG_Init();

	// Don't call D_DoomMain() here - it will be called separately
	// We just mark that we're ready
	doom_initialized = 0;
}

// Initialize Doom's main code (separate from Create)
void doomgeneric_InitMain(void)
{
    if (doom_initialized) {
        return;
    }

    doom_initialized = 1;   // Mark initialized BEFORE entering Doom
    D_DoomMain();           // Never returns
}


