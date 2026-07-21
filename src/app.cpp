#include <orbis/Keyboard.h>
#include "app.h"

#include <ctime>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <unistd.h>

constexpr auto FB_WIDTH = 800;
constexpr auto FB_HEIGHT = 512;
constexpr auto FB_SIZE = (FB_WIDTH * FB_HEIGHT * 4);
constexpr auto FB_ADDR = (0x60000000 - FB_SIZE);
constexpr auto MEM_SIZE = (3 << 27);
constexpr auto DISPLAY_UPDATE_INTERVAL_MS = (1000 / 30); // 30 FPS

int* mem;
int pc = 0, timer = 0;

static inline int kb_poll(s32 kb_handle) {

    static OrbisKeyboardData prev_state{};
    OrbisKeyboardData data{};
    sceKeyboardReadState(kb_handle, &data);
    // check first unpressed key
    for (int i = 0; i < prev_state.nkeys; i++) {
        u16 key = prev_state.keycodes[i];
        if (key == 0) {
            continue;
        }
        bool found = false;
        for (int j = 0; j < data.nkeys; j++) {
            if (key == data.keycodes[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            prev_state = data;
            return -key;
        }
    }
    // check first pressed key
    for (int i = 0; i < data.nkeys; i++) {
        u16 key = data.keycodes[i];
        if (key == 0) {
            continue;
        }
        bool found = false;
        for (int j = 0; j < prev_state.nkeys; j++) {
            if (key == prev_state.keycodes[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            prev_state = data;
            return key;
        }
    }
    prev_state = data;
    return 0;
}

static inline int fetch(void) {
    int raw = mem[pc++];
    return (raw & 1) ? mem[raw / 4] / 4 : raw / 4;
}

extern "C" int pthread_rename_np(pthread_t thread, const char* name);

void App::Run() {
    bool running = true;
    std::mutex fb_mutex{};
    std::atomic<bool> fb_updated = false;
#pragma clang optimize off // me when I'm in a weird bugs competition and my opponent is a PS4 toolchain
    std::thread vm{[&, this]() {
#pragma clang optimize on
        pthread_rename_np(pthread_self(), "vm");
        FILE* f = fopen("/data/homebrew/vmlinux.bootimage", "r");
        if (!f)
            return;
        VAddr dmem;
        mem = reinterpret_cast<s32*>(alloc_memory(
            MEM_SIZE * sizeof(s32), 16_KB, ORBIS_KERNEL_WB_ONION, MemoryProt::CpuReadWrite, dmem));
        memset(mem, 0, MEM_SIZE * 4);
        LOG_INFO("Loaded {:.5} MiB data", fread(mem, 4, MEM_SIZE, f) / 1024.0 / 1024.0 * 4);
        fclose(f);

        u64 last_render = 0;
        int a, b, c;

        do {
            // Fetch next instruction
            a = fetch();
            b = fetch();
            c = fetch();

            if (a == -1) { // Read scan code from keyboard
                mem[b] = kb_poll(kb_handle);
            } else if (b == -1) { // Write text to console
                write(1, &mem[a], 1);
            } else { // Subleq operation
                if (a == 64) {
                    timespec_get((struct timespec*)&mem[64], TIME_UTC); // Update clock
                }

                mem[b] -= mem[a];
                if (mem[b] <= 0) {
                    pc = c;
                }

                if (mem[0] && ++timer > 800000) { // Timer interrupt & display update

                    // Trigger interrupt
                    mem[1] = pc * 4;
                    pc = mem[0] / 4;
                    timer = 0;

                    // Update IO
                    u64 now = sceKernelGetProcessTime() / 1000;
                    if (now - last_render >= DISPLAY_UPDATE_INTERVAL_MS) {
                        last_render = now;
                        {
                            std::scoped_lock l{fb_mutex};
                            for (s32 i = 0; i < FB_SIZE / 4; i++) {
                                s32 x = i % FB_WIDTH * 2;
                                s32 y = i / FB_WIDTH * 2;
                                // renderer.scene->DrawRectangle(x, y, 2, 2, mem[FB_ADDR / 4 + i]);
                                auto dest =
                                    (s32*)renderer.scene
                                        ->frameBuffers[renderer.scene->activeFrameBufferIdx];
                                dest[renderer.scene->width * y + x] = mem[FB_ADDR / 4 + i];
                                dest[renderer.scene->width * y + x + 1] = mem[FB_ADDR / 4 + i];
                                dest[renderer.scene->width * (y + 1) + x] = mem[FB_ADDR / 4 + i];
                                dest[renderer.scene->width * (y + 1) + x + 1] =
                                    mem[FB_ADDR / 4 + i];
                            }
                            fb_updated = true;
                        }
                    }
                }
            }

        } while (c);
        fb_updated = true;
        running = false;
    }};
    vm.detach();

    renderer.BeginFrame();
    while (running && HandleInput()) {
        while (!fb_updated) {
            sceKernelUsleep(1000);
        }
        fb_updated = false;
        std::scoped_lock l{fb_mutex};
        renderer.EndFrame();
        renderer.BeginFrame();
    }
    renderer.EndFrame();
}

App::App() {
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    LOG_INFO("userid: {}", user_id);
    pad.Init(user_id);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_KEYBOARD);
    sceKeyboardInit();
    ASSERT_NO_ERROR(kb_handle = sceKeyboardOpen(user_id, 0, 0, nullptr));
}

App::~App() {
    LOG_INFO("App stopped.");
    sceKeyboardClose(kb_handle);
    sceSystemServiceLoadExec("EXIT", nullptr);
}

bool App::HandleInput() {
    pad.Update();
    if (pad.IsPressed(OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE)) {
        return false;
    }
    if (pad.IsPressed(OrbisPadButton::ORBIS_PAD_BUTTON_TRIANGLE)) {
        LOG_INFO("pc: {:#x}", pc);
    }
    return true;
}

void App::DrawDemo() {
    renderer.scene->DrawRectangle(100, 100, 200, 200, {0, 0, 0});
    renderer.scene->DrawRectangle(120, 120, 160, 160, {255, 128, 128});
    renderer.scene->DrawRectangle(140, 140, 120, 120, {0, 0, 0});
    renderer.scene->DrawText("Hello, Screen!", renderer.font, 400, 220, {50, 50, 50}, {0, 0, 255});
}
