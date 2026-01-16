// Simple kernel-compatible m_argv implementation for Tiny64 Doom

#include "../include/doomtype.h"


// Global argument variables (simplified for kernel use)
int myargc = 0;
char** myargv = 0;

// Minimal stub: always returns 0 (not found)
int M_CheckParm(char* check) { (void)check; return 0; }

// Minimal stub: always returns 0 (not found)
int M_CheckParmWithArgs(char* check, int num_args) { (void)check; (void)num_args; return 0; }

// Minimal stub: always returns false
boolean M_ParmExists(char* check) { (void)check; return false; }

// Minimal stub: does nothing
void M_FindResponseFile(void) { }
