#include "gdt.h"

extern void gdt_flush();

void init_gdt() {
    gdt_flush();
}