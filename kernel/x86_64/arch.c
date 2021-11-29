#include "arch.h"
#include "cpuid.h"
#include "idt.h"
#include "gdt.h"
#include "sse.h" 
#include "memory/mem_virt.h"
#include "memory/mem_phys.h"
#include "device/acpi.h"
#include "kernel/common/device/serial.h"
#include "kernel/common/video/display.h"
#include "kernel/common/kservice.h"
#include "libs/libc/size_t.h"
#include "thirdparty/stivale2hdr.h"

void _kstart(struct stivale2_struct *stivale2_struct) {
    struct stivale2_struct_tag_terminal *term_str_tag;
    struct stivale2_struct_tag_framebuffer *framebuf_str_tag;
    struct stivale2_struct_tag_memmap *memmap_str_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MEMMAP_ID);

    //? Core initialization
    init_serial(COM1);
    init_kservice();
    init_sse();
    init_gdt();
    init_idt();
    init_cpuid();

    //? Memory manager initialization 
    kinit_mem_manager(memmap_str_tag);

    //? ACPI initialization
    init_acpi();

    //? -----------------------------------------
    for (;;) asm("hlt");

    //? Display driver setup
    term_str_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_TERMINAL_ID);
    framebuf_str_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);
    uint32_t checker_result = check_framebuffer_or_terminal(framebuf_str_tag, term_str_tag);

    if (checker_result == CHECKER_NEITHER_AVAILABLE) {
        ks.panic("The machine could neither provide a framebuffer address or a terminal interface. Cannot continue, halting system.");

    } else if (checker_result == CHECKER_FRAMEBUFFER_AVAILABLE) {
        ks.dbg("Initializing video driver... ");
        if (init_video_driver(framebuf_str_tag->framebuffer_addr, framebuf_str_tag->framebuffer_width, framebuf_str_tag->framebuffer_height, 
                        framebuf_str_tag->framebuffer_pitch, framebuf_str_tag->framebuffer_bpp, framebuf_str_tag->red_mask_size, framebuf_str_tag->green_mask_size, 
                        framebuf_str_tag->blue_mask_size, framebuf_str_tag->red_mask_shift, framebuf_str_tag->green_mask_shift, framebuf_str_tag->blue_mask_shift))
            ks.dbg("Video driver initialized. Framebuffer at %x", framebuf_str_tag->framebuffer_addr);
    
    } else if (checker_result == CHECKER_TERMINAL_AVAILABLE) {
        void *term_write_ptr = (void *)term_str_tag->term_write;
        void (*term_write)(const char *string, size_t length) = term_write_ptr;

        term_write("\n \
        _   _            _        _               _  __                    _  \n \
        | \\ | |          | |      (_)             | |/ /                   | | \n \
        |  \\| | ___ _   _| |_ _ __ _ _ __   ___   | ' / ___ _ __ _ __   ___| | \n \
        | . ` |/ _ \\ | | | __| '__| | '_ \\ / _ \\  |  < / _ \\ '__| '_ \\ / _ \\ | \n \
        | |\\  |  __/ |_| | |_| |  | | | | | (_) | | . \\  __/ |  | | | |  __/ | \n \
        |_| \\_|\\___|\\__,_|\\__|_|  |_|_| |_|\\___/  |_|\\_\\___|_|  |_| |_|\\___|_|", 461);
        term_write("v0.1-dev", 8);

        term_write("\n\n", 2);
        term_write("Hello world", 11);
    }
    //? -----------------------------------------

    //TODO: implement user-space (final goal)
    for (;;) asm("hlt");
}

void kinit_mem_manager(struct stivale2_struct_tag_memmap* memmap_str_tag) {
    uint32_t memmap_entries = memmap_str_tag->entries;
    uint64_t n_base, n_size;
    int i = 0, lookahead = 1;
    
    struct memory_physical_region entries[memmap_entries];
    while (i < memmap_entries) {
        struct stivale2_mmap_entry *entry = memmap_str_tag->memmap + i;

        entries[i].base = entry->base;
        entries[i].size = entry->length;
        entries[i].limit = entry->base + entry->length;
        
        if (entry->type == STIVALE2_MMAP_USABLE || entry->type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE) entries[i].type = MEMORY_REGION_USABLE;
        else if (entry->type == STIVALE2_MMAP_KERNEL_AND_MODULES) entries[i].type = MEMORY_REGION_KERNEL;
        else if (entry->type == STIVALE2_MMAP_FRAMEBUFFER) entries[i].type = MEMORY_REGION_FRAMEBUFFER;
        else if (entry->type == STIVALE2_MMAP_ACPI_NVS) entries[i].type = MEMORY_REGION_ACPI_RSVD;
        else if (entry->type == STIVALE2_MMAP_ACPI_RECLAIMABLE) entries[i].type = MEMORY_REGION_ACPI_RCLM;
        else entries[i].type = MEMORY_REGION_RESERVED;

        if (i+lookahead < memmap_entries) {
            while ((entries[i].type == MEMORY_REGION_USABLE && (entry+lookahead)->type == STIVALE2_MMAP_USABLE || 
                    (entry+lookahead)->type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE)) {
                        entries[i].size += (entry+lookahead)->length;
                        entries[i].limit = (entry+lookahead)->base + (entry+lookahead)->length;
                        entries[i+lookahead].type = MEMORY_REGION_INVALID;
                        lookahead++;
            }
            i += lookahead;
            lookahead = 1;
        } else {
            i++;
        }
    }

    init_pmm(entries, memmap_entries);
    init_vmm();
}
