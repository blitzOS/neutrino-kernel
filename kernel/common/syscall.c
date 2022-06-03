#include "syscall.h"
#include "arch.h"
#include "kservice.h"
#include "tasks/task.h"
#include "tasks/scheduler.h"
#include <neutrino/syscall.h>
#include <stdint.h>

// === PRIVATE FUNCTIONS ========================

SyscallResult sys_test(uintptr_t* args) {
    ks.dbg("Hello syscall world!");
    return SYSCALL_SUCCESS;
}

SyscallResult sys_destroy_task(uintptr_t* args) {
    sched_terminate();
    return SYSCALL_FAILURE; // return failure on sched_terminate return 
}

SyscallResult sys_now(SCNowArgs* args) {
    *args = (SCNowArgs){
        .timestamp = arch_now(),
    };

    return SYSCALL_SUCCESS;
}

// === PUBLIC FUNCTIONS =========================

typedef SyscallResult SyscallFn();

SyscallFn* syscalls[NEUTRINO_SYSCALL_COUNT] = {
    [NEUTRINO_TEST] = sys_test,
    [NEUTRINO_NOW] = sys_now,
    [NEUTRINO_KILL_TASK] = sys_destroy_task
};

SyscallResult syscall_execute(NeutrinoSyscall syscall_id, uintptr_t* args) {
    if (syscall_id >= NEUTRINO_SYSCALL_COUNT)
        return SYSCALL_INVALID;
    
    task_start_syscall();

    SyscallResult result = syscalls[syscall_id](args);

    const Task* cur = get_current_task();
    ks.log("Syscall: #%u in task #%u (%c)", syscall_id, cur->pid, cur->name);

    task_end_syscall();

    return result;
}
