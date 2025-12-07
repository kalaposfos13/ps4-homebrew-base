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

extern "C" int sceKernelInstallExceptionHandler(s32 sig_num, void (*handler)(int, void*));
extern "C" int sceKernelRemoveExceptionHandler(s32 sig_num);

void swap_handler(int sig, void* raw_context) {
    auto& mctx = ((ucontext_t*)raw_context)->uc_mcontext;
    LOG_INFO("addr: {:#x}", (mctx.gregs[REG_TRAPNO]));
    void* aligned_addr = (void*)((uintptr_t(mctx.gregs[REG_TRAPNO])) & ~0xfff);
    void* res =
        mmap(aligned_addr, 4096, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);
    if (res == MAP_FAILED) {
        LOG_INFO("mmap returned {} {}", errno, strerror(errno));
    }
}

int main(int argc, char* argv[]) {
    size_t len = 4096 * 4096;
    void* addr = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_VOID | MAP_PRIVATE, -1, 0);
    ASSERT(addr != MAP_FAILED);
    LOG_INFO("base: {}\n", fmt::ptr(addr));

    sceKernelInstallExceptionHandler(SIGSEGV, &swap_handler);

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

    // optional
    sceKernelRemoveExceptionHandler(SIGSEGV);

    LOG_INFO("Exiting");
    sceSystemServiceLoadExec("exit", nullptr);
    return EXIT_SUCCESS;
}