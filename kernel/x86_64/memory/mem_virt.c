#include "mem_virt.h"
#include "mem_phys.h"
#include "paging.h"
#include "../smp.h"
#include "../cpuid.h"
#include "../arch.h"
#include "kernel/common/kservice.h"
#include "kernel/common/memory/memory.h"
#include <stdbool.h>
#include <thirdparty/stivale2.h>

// === PRIVATE FUNCTIONS ========================

// *Get a page entry in the given page table
// @param table the page table to search the entry in
// @param entry the entry to be found
// @return a pointer to the page entry requested, or 0 if not found
page_table* vmm_get_entry(page_table* table, uint64_t entry) {
    if (IS_PRESENT(table->entries[entry])) 
        return get_mem_address(GET_PHYSICAL_ADDRESS(table->entries[entry]));
    return 0;
}

// *Create a new page table entry in the given table at the given entry with the specified writable and user flags
// @param table the table to add the entry to
// @param entry the index of the entry to be added
// @param writable flag to indicate whether the newly created entry should be writable or not
// @param user flags to indicate whether the newly created entry should be accessible from userspace or not
// @return the address of the newly created entry
page_table* vmm_create_entry(page_table* table, uint64_t entry, bool writable, bool user) {
    page_table* pt = (page_table*)get_mem_address(pmm_alloc());
    memory_set(pt, 0, PHYSMEM_BLOCK_SIZE);
    table->entries[entry] = page_create(get_rmem_address(pt), writable, user);

    // ks.dbg("vmm_create_entry() : table: %x entry: %u new_page: %x (%x)", table, entry, pt, get_rmem_address(pt));
    return pt;
}

// *Get or create the entry in the given page table. If the entry already exists in the page table, its address
// *is returned. Otherwise a new entry is created and returned.
// @param table the table to add the entry to
// @param entry the index of the entry to be added
// @param writable flag to indicate whether the newly created entry should be writable or not
// @param user flags to indicate whether the newly created entry should be accessible from userspace or not
// @return the address of the existing table or the newly created table
page_table* vmm_get_or_create_entry(page_table* table, uint64_t entry, bool writable, bool user) {
    if (IS_PRESENT(table->entries[entry])) {
        return get_mem_address(GET_PHYSICAL_ADDRESS(table->entries[entry]));
    } else {
        return vmm_create_entry(table, entry, writable, user);
    }
}

// *Return the physical address given a virtual address
// @param virt the virtual address
// @return the physical address associated to the virtual address
uint64_t vmm_get_phys_address(uint64_t virt) {
    page_table* pdpt = vmm_get_entry(vmm.kernel_page_physaddr, (uint64_t)GET_PL4_INDEX(virt));
    if (pdpt == 0) ks.warn("Physical address for virtual address %x is invalid", virt);
    
    page_table* pdir = vmm_get_entry(pdpt, (uint64_t)GET_DPT_INDEX(virt));
    if (pdir == 0) ks.warn("Physical address for virtual address %x is invalid", virt);
   
    page_table* ptab = vmm_get_entry(pdir, (uint64_t)GET_DIR_INDEX(virt));
    if (ptab == 0) ks.warn("physical address for virtual address %x is invalid", virt);

    return GET_PHYSICAL_ADDRESS(ptab->entries[(uint64_t)GET_TAB_INDEX(virt)]) + GET_PAGE_OFFSET(virt);
}

// *Return a new page table of 512 entries all set to not-present 
// @return the new page table pointer
page_table *vmm_new_table() {
    page_table *pl4 = (page_table*) get_mem_address(pmm_alloc());
    memory_set(pl4, 0, PHYSMEM_BLOCK_SIZE);
    for (int i = 0; i < PAGE_PL4_ENTRIES; i++) pl4->entries[i] = 0;

    return pl4;
}

// *Refresh paging by reloading the CR3 register
void vmm_refresh_paging() {
    __asm__("mov %%cr3, %%rax" : : );
    __asm__("mov %%rax, %%cr3" : : );
}

// *Check if a page table is free in each of its entries
// @param table the page table to check
// @return true if the page table is free, false otherwise
bool vmm_is_table_free(page_table* table) {
    for (int i = 0; i < 512; i++) {
        if (vmm_get_entry(table, i) != 0) return false;
    }

    return true;
}

// *Free a physical memory block if the page pointing to it is free
// @param table the table to check if is free
// @param parent the parent page of the table
// @param parent_index the index of [table] in the [parent] page
void vmm_free_if_necessary_table(page_table* table, page_table* parent, uint64_t parent_index) {
    if (vmm_is_table_free(table)) {
        pmm_free(GET_PHYSICAL_ADDRESS(parent->entries[parent_index]));
        page_clear_bit(vmm_get_entry(table, parent_index), PRESENT_BIT_OFFSET);
    }
}

// *Free all the tables in the pml4 table where the given address is free
// @param table the pml4 table to check the address in
// @param address the address unmapped from the pml4 table
void vmm_free_if_necessary_tables(page_table* table, uint64_t unmapped_addr) {
    uint64_t pl4_entry = GET_PL4_INDEX(unmapped_addr);
    uint64_t dpt_entry = GET_DPT_INDEX(unmapped_addr);
    uint64_t pd_entry = GET_DIR_INDEX(unmapped_addr);
    
    page_table *pdpt, *pd, *pt;
    pdpt = vmm_get_or_create_entry(table, pl4_entry, true, true);
    pd = vmm_get_or_create_entry(pdpt, dpt_entry, true, true);
    pt = vmm_get_or_create_entry(pd, pd_entry, true, true);

    vmm_free_if_necessary_table(pt, pd, pd_entry);
    vmm_free_if_necessary_table(pd, pdpt, dpt_entry);
    vmm_free_if_necessary_table(pdpt, table, pl4_entry);
}

// *Map the physical regions stored in the physical manager to the given page
// @param phys the physical memory manager
// @param page the page to map the regions into
void vmm_map_physical_regions(struct memory_physical* phys, page_table* page) {
    for (int ind = 0; ind < phys->regions_count; ind++) {
        if (phys->regions[ind].type == MEMORY_REGION_USABLE ||
            phys->regions[ind].type == MEMORY_REGION_INVALID ||
            phys->regions[ind].type == MEMORY_REGION_FRAMEBUFFER) continue;

        uint64_t aligned_base = phys->regions[ind].base - phys->regions[ind].base % PAGE_SIZE;
        uint64_t aligned_size = ((phys->regions[ind].size / PAGE_SIZE) + 1) * PAGE_SIZE;

        ks.dbg("region is %i bytes long, needing %i pages. Aligned base is %x, aligned length is %x", 
            phys->regions[ind].size, phys->regions[ind].size / PAGE_SIZE + 1, aligned_base, aligned_size);

        for (int i = 0; i * PAGE_SIZE < aligned_size; i++) {
            uint64_t addr = aligned_base + i * PAGE_SIZE;
            vmm_map_page(page, addr, addr, true, false);
            if (phys->regions[ind].type == MEMORY_REGION_KERNEL) {
                vmm_map_page(page, addr, get_kern_address(addr), true, false);
            } 
        } 
    }
}

// === PUBLIC FUNCTIONS =========================

void init_vmm() {
    ks.log("Initializing VMM...");

    vmm.address_size = get_physical_address_length();
    vmm.stivale2_page_physaddr = (page_table*) read_cr3();
    vmm.kernel_page_physaddr = vmm.stivale2_page_physaddr;

    // prepare a pml4 table for the kernel address space
    page_table* kernel_pml4 = vmm_new_table();
    ks.dbg("New pml4 created at %x", kernel_pml4);

    // identity map the first 2M of physical memory
    for (int i = 0; i < PHYSMEM_2MEGS / PAGE_SIZE; i++)
        vmm_map_page(kernel_pml4, i*PAGE_SIZE, i*PAGE_SIZE, true, false);

    // map the page itself in the 510st pml4 entry
    ks.dbg("mapping page map inside itself: %x (%x) to %x", kernel_pml4, get_rmem_address((uint64_t)kernel_pml4), RECURSE_PML4);
    kernel_pml4->entries[RECURSE] = page_create(get_rmem_address((uint64_t)kernel_pml4), true, false);

    // map physical regions in the page
    vmm_map_physical_regions(&pmm, kernel_pml4);

    // map the pmm map
    uint64_t aligned_base = (uint64_t)pmm._map - (uint64_t)pmm._map % PAGE_SIZE;
    uint64_t aligned_size = ((pmm._map_size / PAGE_SIZE) + 1) * PAGE_SIZE;
    ks.dbg("Aligned base is %x, aligned length is %x", aligned_base, aligned_size);

    for (int i = 0; i * PAGE_SIZE < aligned_size; i++) {
        uint64_t addr = aligned_base + i * PAGE_SIZE;
        vmm_map_page(kernel_pml4, get_rmem_address(addr), addr, true, false);
    }
    
    // give CR3 the kernel pml4 address
    ks.dbg("Preparing to load pml4...");
    vmm.kernel_page_physaddr = get_rmem_address((uint64_t)kernel_pml4); 
    get_bootstrap_cpu()->page_table = get_rmem_address((uint64_t)kernel_pml4);
    write_cr3(get_rmem_address((uint64_t)kernel_pml4));
    ks.log("VMM has been initialized.");
}

void init_vmm_on_ap(struct stivale2_smp_info* info) {
    ks.log("Initializing VMM on CPU #%u...", info->processor_id);

    // prepare a pml4 table for the kernel address space
    page_table* kernel_pml4 = vmm_new_table();
    ks.dbg("New pml4 created at %x for CPU #%u", kernel_pml4, info->processor_id);

    // identity map the first 2M of physical memory
    for (int i = 0; i < PHYSMEM_2MEGS / PAGE_SIZE; i++)
        vmm_map_page(kernel_pml4, i*PAGE_SIZE, i*PAGE_SIZE, true, false);

    // map the page itself in the 510th pml4 entry
    ks.dbg("mapping page map inside itself: %x (%x) to %x", kernel_pml4, get_rmem_address((uint64_t)kernel_pml4), RECURSE_PML4);
    kernel_pml4->entries[RECURSE] = page_create(get_rmem_address((uint64_t)kernel_pml4), true, false);

    // map kernel in the page
    vmm_map_physical_regions(&pmm, kernel_pml4); 

    // map the cpu stack in the page
    uint64_t stack_base = (uint64_t)(info->target_stack-CPU_STACK_SIZE) - (uint64_t)(info->target_stack - CPU_STACK_SIZE) % PAGE_SIZE;
    uint64_t stack_size = ((CPU_STACK_SIZE / PAGE_SIZE) + 1) * PAGE_SIZE;
    ks.dbg("Mapping CPU stack starting %x, length %x", stack_base, stack_size);
    for (int i = 0; i * PAGE_SIZE < stack_size; i++) {
        uint64_t addr = stack_base + i * PAGE_SIZE;
        vmm_map_page(kernel_pml4, get_rmem_address(addr), addr, true, false);
    }

    // map the pmm map
    uint64_t aligned_base = (uint64_t)pmm._map - (uint64_t)pmm._map % PAGE_SIZE;
    uint64_t aligned_size = ((pmm._map_size / PAGE_SIZE) + 1) * PAGE_SIZE;
    ks.dbg("Aligned base is %x, aligned length is %x", aligned_base, aligned_size);

    for (int i = 0; i * PAGE_SIZE < aligned_size; i++) {
        uint64_t addr = aligned_base + i * PAGE_SIZE;
        vmm_map_page(kernel_pml4, get_rmem_address(addr), addr, true, false);
    }
    
    // give CR3 the kernel pml4 address
    ks.dbg("Preparing to load pml4... %x %x", kernel_pml4, get_rmem_address((uint64_t)kernel_pml4));
    get_cpu(info->processor_id)->page_table = get_rmem_address((uint64_t)kernel_pml4);
    write_cr3(get_rmem_address((uint64_t)kernel_pml4));
    ks.log("VMM has been initialized.");
}

// *Map a physical address to a virtual page address
// @param table the table to map the address into
// @param phys_addr the physical address to map the virtual address to
// @param virt_addr the virtual address to map the physical address to
// @param writable flag to indicate whether the newly created entry should be writable or not
// @param user flags to indicate whether the newly created entry should be accessible from userspace or not
void vmm_map_page(page_table* table, uint64_t phys_addr, uint64_t virt_addr, bool writable, bool user) {
    pagingPath path = GetPagingPath(virt_addr);

    page_table *pdpt, *pd, *pt;
    pdpt = vmm_get_or_create_entry(table, path.pl4, true, true);
    pd = vmm_get_or_create_entry(pdpt, path.dpt, true, true);
    pt = vmm_get_or_create_entry(pd, path.pd, true, true);

    pt->entries[path.pt] = page_create(phys_addr, writable, user);
}

// *Unmap a page given the table and a virtual address in the page
// @param table the pml4 table the virtual address resides in
// @param virt_addr the virtual address in the page
// @return true if the page was successfully unmapped, false otherwise
bool vmm_unmap_page(page_table* table, uint64_t virt_addr) {
    pagingPath path = GetPagingPath(virt_addr);
    
    page_table *pdpt, *pd, *pt;
    pdpt = vmm_get_or_create_entry(table, path.pl4, true, true);
    pd = vmm_get_or_create_entry(pdpt, path.dpt, true, true);
    pt = vmm_get_or_create_entry(pd, path.pd, true, true);

    page_table *to_free = vmm_get_entry(pt, path.pt);
    if (to_free == 0) return false;

    page_clear_bit(to_free, PRESENT_BIT_OFFSET);

    vmm_free_if_necessary_tables(table, virt_addr);
    vmm_refresh_paging();
    return true;
}

// *Map a physical address to a virtual page address in the current table and refresh the TLB
// @param phys_addr the physical address to map the virtual address to
// @param virt_addr the virtual address to map the physical address to
// @param writable flag to indicate whether the newly created entry should be writable or not
// @param user flags to indicate whether the newly created entry should be accessible from userspace or not
void vmm_map_page_in_active_table(uint64_t phys_addr, uint64_t virt_addr, bool writable, bool user) {
    pagingPath path = GetPagingPath(virt_addr);
    volatile page_table_e* p;

    p = GET_RECURSIVE_ADDRESS(RECURSE, RECURSE, RECURSE, path.pl4);
    if (!*p) *p = page_create((uint64_t)pmm_alloc(), writable, user);

    p = GET_RECURSIVE_ADDRESS(RECURSE, RECURSE, path.pl4, path.dpt);
    if (!*p) *p = page_create((uint64_t)pmm_alloc(), writable, user);

    p = GET_RECURSIVE_ADDRESS(RECURSE, path.pl4, path.dpt, path.pd);
    if (!*p) *p = page_create((uint64_t)pmm_alloc(), writable, user);

    p = GET_RECURSIVE_ADDRESS(path.pl4, path.dpt, path.pd, path.pt); 
    *p = page_create(phys_addr, writable, user); 

    vmm_refresh_paging();
}

// *Unmap a page given the virtual address in the page in the active table
// @param virt_addr the virtual address in the page
// @return true if the page was successfully unmapped, false otherwise
bool vmm_unmap_page_in_active_table(uint64_t virt_addr) {
    pagingPath path = GetPagingPath(virt_addr);
    volatile page_table_e* to_free = GET_RECURSIVE_ADDRESS(path.pl4, path.dpt, path.pd, path.pt); 
    if (to_free == 0) return false;

    page_clear_bit(to_free, PRESENT_BIT_OFFSET);
    vmm_refresh_paging();

    return true;
}

// *Map a physical address to a virtual address. Works with either an offline page table or an active one
// @param table the table to map the address into
// @param phys_addr the physical address to map the virtual address to
// @param virt_addr the virtual address to map the physical address to
// @param writable flag to indicate whether the newly created entry should be writable or not
// @param user flags to indicate whether the newly created entry should be accessible from userspace or not
void vmm_allocate_memory(page_table_e* table, size_t blocks, uint64_t virt_addr, bool writable, bool user) {
    uint64_t phys_addr = (uint64_t)pmm_alloc_series(blocks);

    for (size_t i = 0; i < blocks; i++) {
        if (table == USE_ACTIVE_PAGE) vmm_map_page_in_active_table(phys_addr + (i*PHYSMEM_BLOCK_SIZE), virt_addr, writable, user);
        else vmm_map_page(table, phys_addr + (i*PHYSMEM_BLOCK_SIZE), virt_addr, writable, user);
    }   
}

// *Map a MMIO physical address to itself. Works with either an offline page table or an active one
// @param table the table to map the address into
// @param mmio_addr the memory mapped IO address to map
void vmm_map_mmio(uint64_t mmio_addr, size_t blocks) {
    for (size_t i = 0; i < blocks; i++) 
        vmm_map_page_in_active_table(mmio_addr + (i*PHYSMEM_BLOCK_SIZE), mmio_addr + (i*PHYSMEM_BLOCK_SIZE), true, false);
}
