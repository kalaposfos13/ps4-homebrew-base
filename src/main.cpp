#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#include <sys/mman.h>

#define _GNU_SOURCE 1
#include <signal.h>
#include <ucontext.h>

#include "assert.h"
#include "logging.h"
#include "types.h"


#define MAP_VOID 0x100

extern "C" int sceKernelInstallExceptionHandler(s32 sig_num, void (*handler_func)(int, void*));

struct sigaction old_sa_segv;
void swap_handler(int sig, void *raw_context) {
    auto& mctx = ((ucontext_t*)raw_context)->uc_mcontext;
    LOG_INFO("si: {:#x}", (mctx.gregs[20]));
    void* aligned_addr = (void*)((uintptr_t(mctx.gregs[REG_TRAPNO])) & ~0xfff);
    void* res = mmap(aligned_addr, 4096, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);
    LOG_INFO("res != MAP_FAILED = {}", res != MAP_FAILED);
}

int main(int argc, char *argv[]) {
    size_t len = 4096 * 4096;
    void* addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_VOID | MAP_PRIVATE, -1, 0);
    ASSERT(addr != MAP_FAILED);
    LOG_INFO("base: {}\n", fmt::ptr(addr));

    sceKernelInstallExceptionHandler(SIGSEGV, (void(*)(int, void*))&swap_handler);

    printf("base: %p\n", addr);
    {
        uint8_t volatile* addr_u8 = (uint8_t volatile*)addr;

        ASSERT(addr_u8[0] == 0);
        addr_u8[0] = 1;
        ASSERT(addr_u8[0] == 1);

        addr_u8[4096] = 2;
        ASSERT(addr_u8[4096] == 2);
        ASSERT(addr_u8[0] == 1);
    }

    munmap(addr, len);
    LOG_INFO("Exiting");
    sceSystemServiceLoadExec("exit", nullptr);
    return EXIT_SUCCESS;
}