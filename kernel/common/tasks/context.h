#pragma once

#include <stdint.h>

#ifdef __x86_64
#include "kernel/x86_64/tasks/context.h"
#else
#error "Unsupported platform"
#endif

typedef struct __context Context;
typedef struct __context_flags {
    uint8_t user : 1;
    uint64_t padding : 63;
} ContextFlags;

#define IsUserTask(flags) (flags.user == 1)

Context* NewContext();
void DestroyContext(Context* context);
void context_init(Context* context, uintptr_t ip, uintptr_t sp, uintptr_t ksp, ContextFlags cflags);
