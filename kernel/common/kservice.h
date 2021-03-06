#pragma once
#include <neutrino/lock.h>

enum KERNEL_FATAL {
    //! reserved                    | 0x000 - 0x0ff
    INTERNAL_TEST       = 0x000,
    NOT_IMPLEMENTED     = 0x001,
    FATAL_ERROR         = 0x002,

    //! core hardware not found     | 0x100 - 0x1ff
    NO_PIT              = 0x100,
    NO_FRAMEBUFFER      = 0x101,
    NO_LAPIC            = 0x102,

    //! fatal exceptions            | 0x200 - 0x2ff
    GENERIC_EXCEPTION   = 0x200,
    INTERRUPT_EXCEPTION = 0x201,
    OUT_OF_MEMORY       = 0x202,
    OUT_OF_HEAP         = 0x203,
};

typedef struct __fatal {
    enum KERNEL_FATAL code;
    char* message;
} Fatal;

#define FatalError(code, message) (Fatal){code, message}

struct KernelService {
    void (*dbg)     (char* message, ...);
    void (*log)     (char* message, ...);
    void (*warn)    (char* message, ...);
    void (*err)     (char* message, ...);
    void (*fatal)   (Fatal fatal_error, ...);

    void (*_put) (char* message, ...);
    void (*_helper) (char* message);
    Lock lock;
};

enum KSERVICE_TYPE {
    KSERVICE_HELPER,
    KSERVICE_LOG,
    KSERVICE_DEBUG,
    KSERVICE_WARNING,
    KSERVICE_ERROR,
    KSERVICE_FATAL
};

//? Global KernelService
struct KernelService ks;

void init_kservice();
void set_kservice(enum KSERVICE_TYPE, void (*));
