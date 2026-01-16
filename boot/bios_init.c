// BIOS boot initialization
// Minimal setup for BIOS-booted kernel

void bios_boot_init(void) {
    // GDT and segment setup is already handled in stage2.S
    // This function can be used for any additional BIOS-specific initialization
}
