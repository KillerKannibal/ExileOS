#include <stdint.h>
#include "gdt.h"
#include "idt.h"
#include "multiboot.h"
#include "gui.h"
#include "wm.h"
#include "input.h"
#include "fs.h"
#include "pci.h"
#include "debug.h"
#include "globals.h"
#include "net.h"
#include "rtl8139.h"
#include "browser.h"
#include "rndis.h"

void ___chkstk_ms() {}
void __chkstk_ms() {}

void kernel_main(struct multiboot_info* mbinfo) {
    debug_init(); 
    global_mbinfo = mbinfo;
    kprint_serial("\n[BOOT] --- ExileOS Kernel Starting ---\n");

    // Initialize CPU structures
    kprint_serial("[BOOT] Initializing GDT/TSS...\n");
    init_gdt();
    uint32_t esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));
    tss.esp0 = esp;
    kprint_serial("[BOOT] Initializing IDT...\n");
    init_idt();

    // Setup Video
    if (mbinfo->flags & (1 << 12)) {
        init_vga(mbinfo);
        kprint_serial("VGA LFB Initialized\n");
    } else {
        kprint_serial("Error: No VBE LFB found in multiboot info\n");
        while(1);
    }

    // Early Splash Screen
    draw_rect(0, 0, screen_width, screen_height, 0x000000);
    draw_string("ExileOS is loading components...", 20, 20, 0xFFFFFF);
    screen_update();

    // Drivers & Filesystem
    kprint_serial("[BOOT] Mounting Filesystem...\n");
    draw_string(" - Initializing Filesystem", 20, 40, 0xAAAAAA);
    screen_update();

    uint32_t mod_start = 0;
    if (mbinfo->flags & (1 << 3) && mbinfo->mods_count > 0) {
        mod_start = ((multiboot_module_t*)mbinfo->mods_addr)->mod_start;
    }
    init_fs(mod_start);
    
    kprint_serial("[BOOT] Initializing PS/2 Mouse...\n");
    mouse_install();
    
    kprint_serial("[BOOT] Probing PCI Bus...\n");
    pci_init();

    kprint_serial("[BOOT] Starting Network Stack...\n");
    net_init();
    rtl8139_init();
    browser_init();
    kprint_serial("[BOOT] Initialization Complete.\n");
    
    __asm__ volatile("sti"); // Enable hardware interrupts

    // Start Desktop
    init_desktop_buffer(wallpaper_mode);
    open_window(MODE_SHELL);

    // Executive Loop
    while(1) {
        wm_update();      // Logic: process mouse, dragging, and focus
        
        gui_prepare_frame(); // Render: Background wallpaper
        wm_draw_all();       // Render: All open windows
        
        draw_taskbar();      // Render: System UI
        if (start_menu_open) draw_start_menu();
        
        draw_cursor(mouse_x, mouse_y); // Render: Topmost layer
        
        screen_update();     // Flush backbuffer to physical screen
    }
}