#include "gdt.h"

extern void gdt_flush();
extern void tss_flush();

void init_gdt() {
    gdt_flush();
    tss_flush();
}