#include "space.h"
#include "mem_virt.h"
#include "kernel/common/memory/space.h"
#include "kernel/common/memory/memory.h"
#include <liballoc.h>
#include <_null.h>

// === PRIVATE FUNCTIONS ========================

// === PUBLIC FUNCTIONS =========================

Space* NewSpace() {
    Space* space = (Space*)kmalloc(sizeof(Space));
    space->page_table = NewPageTable();
    return space;
}

void DestroySpace(Space* space) {
    if (space->memory_ranges != nullptr) {

        MemoryRangeNode* node = space->memory_ranges;
        while (node->next != nullptr) { // loop and free the memory ranges
            vmm_free_memory(space->page_table, node->range.base, node->range.size);
            kfree(node);
            node = node->next;
        }
        
    }

    DestroyPageTable(space->page_table);
    kfree(space);
}

void space_switch(Space* space) {
    lock(&space->lock);
    vmm_switch_space(space->page_table);
    unlock(&space->lock);
}

uintptr_t space_map(Space* space, uintptr_t phys_addr, size_t size, uintptr_t virt_addr) {
    lock(&space->lock);

    // map all the required pages
    for (uint32_t i = 0; i < size; i++) {
        vmm_map_page(space->page_table, phys_addr, virt_addr, true, true);
    }

    // create a new node and attach it to the list 
    MemoryRangeNode* new_node = (MemoryRangeNode*)kmalloc(sizeof(MemoryRangeNode));
    new_node->range = (MemoryRange){virt_addr, size};
    new_node->next = space->memory_ranges;
    space->memory_ranges = new_node;

    unlock(&space->lock);
}

void space_unmap(Space* space, uintptr_t virt_addr) {
    lock(&space->lock);

    // find the corresponding node and free the associated memory range
    MemoryRangeNode* node = space->memory_ranges;
    while (node->next != nullptr) { 
        if (node->range.base != virt_addr) {
            node = node->next;
            continue;
        }
        
        vmm_free_memory(space->page_table, node->range.base, node->range.size);
        kfree(node);        
    }

    unlock(&space->lock);
}
