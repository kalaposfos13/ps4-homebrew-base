#include "app.h"

#include <map>
#include <thread>
#include <utility>
#include <fcntl.h>

extern "C" {
// s32 PS4_SYSV_ABI open(const char* filename, s32 flags, u16 mode);
s64 PS4_SYSV_ABI read(s32 fd, void* buf, u64 nbytes);
s32 PS4_SYSV_ABI close(s32 fd);
}

void App::Run() {
    u64 const fsize = 1048576000ull;
    VAddr dmem = 0;
    bool started = false;

    char* filebuf =
        (char*)alloc_memory(fsize, 16_KB, ORBIS_KERNEL_WC_GARLIC, MemoryProt::CpuReadWrite, dmem);
    std::thread protector{[&]() {
        constexpr u64 protect_size = 16_KB;
        LOG_INFO("starting protect spam");
        started = true;
        for (int i = 0; i < fsize; i += protect_size) {
            ASSERT_OK(sceKernelMprotect(filebuf + i, protect_size,
                                        MemoryProt::CpuReadWrite | MemoryProt::GpuRead));
            if (i / 16_KB % 100 == 0) {
                sceKernelUsleep(5);
            }
        }
        LOG_INFO("finished protect spam");
    }};
    while (!started) {
    }
    sceKernelUsleep(50);
    s32 fh = open("/app0/testfile", 0, 0666);
    read(fh, filebuf, fsize);
    close(fh);
    protector.join();
    for (int i = 0; i < fsize - 3; i += 3) {
        if (!(filebuf[i] == 'A' && filebuf[i + 1] == 'a' && filebuf[i + 2] == '\n')) {
            UNREACHABLE_MSG("deviation at {}", i);
        }
    }
    LOG_INFO("test passed");
}

App::App() {
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    LOG_INFO("userid: {}", user_id);
    pad.Init(user_id);
}

App::~App() {
    LOG_INFO("App stopped.");
    sceSystemServiceLoadExec("EXIT", nullptr);
}

bool App::HandleInput() {
    pad.Update();
    if (pad.IsPressed(OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE)) {
        return false;
    }
    return true;
}

void App::DrawDemo() {
    renderer.scene->DrawRectangle(100, 100, 200, 200, {0, 0, 0});
    renderer.scene->DrawRectangle(120, 120, 160, 160, {255, 128, 128});
    renderer.scene->DrawRectangle(140, 140, 120, 120, {0, 0, 0});
    renderer.scene->DrawText("Hello, Screen!", renderer.font, 400, 220, {50, 50, 50}, {0, 0, 255});
}
