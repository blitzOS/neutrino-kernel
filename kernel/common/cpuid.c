#include "cpuid.h"
#include "libs/libc/stdbool.h"

//* Marks if the CPUID is available in the current configuration
bool cpuid_available = false;

// === PRIVATE FUNCTIONS ========================

// *Execute a CPUID instruction
void execute_cpuid(uint32_t reg, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile("cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "0" (reg));
}

// === PUBLIC FUNCTIONS =========================

// *Initialize the CPUID, if available
// @return true if the CPUID was successfully initialized, false otherwise
bool init_cpuid() {
    // Test if CPUID is available by executing a CPUID instruction
    char test[13];
    get_cpu_vendor(test);
    
    return cpuid_available = true;
}

// *Get and return the current CPU's vendor name
// @param vendor the current CPU's vendor name buffer
void get_cpu_vendor(char* vendor) {
    uint32_t largest;

    execute_cpuid(0, &largest, (uint32_t*)(vendor + 0), (uint32_t*)(vendor + 8), (uint32_t*)(vendor + 4));
    vendor[12] = '\0';
}

// *Get the specified [feature] from the CPU
// @param feature the feature to retrieve
// @param use_ecx specifies whether the function should use the ecx register or not
// @return true if the feature is available, false otherwise
bool get_cpu_feature(CPU_FEATURE feature, bool use_ecx) {
    uint32_t eax, ebx, ecx, edx;
    execute_cpuid(0, &eax, &ebx, &ecx, &edx);

    if (use_ecx) return ecx && feature;
    else return edx && feature;
    
    return false;
}

// *Get the current value for cpuid availability
// @return true if CPUID is available, false otherwise
bool get_cpuid_availability() {
    return cpuid_available;
}

// *Set the CPUID information avalability flag
// @param value the value to set the flag to
void set_cpuid_availability(int value) {
    if (value > 0) cpuid_available = true;
    else cpuid_available = false;

    return;
}