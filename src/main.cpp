#include <cstring>
#include <sstream>
#include <string>
#include <fcntl.h>
#include <stdio.h>
#include "orbis/AudioOut.h"
#include "orbis/Keyboard.h"
#include "orbis/Pad.h"
#include "orbis/SystemService.h"
#include "orbis/UserService.h"
#include "orbis/libkernel.h"

#include "av_player.h"
#include "graphics.h"

#include "logging.h"

#include <mutex>
#include <thread>
#include <unordered_map>
#include "assert.h"

enum MemoryProt : u32 {
    NoAccess = 0,
    CpuRead = 1,
    CpuWrite = 2,
    CpuReadWrite = 3,
    CpuExec = 4,
    GpuRead = 16,
    GpuWrite = 32,
    GpuReadWrite = 48,
};
template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, std::size_t size) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    auto mod{static_cast<T>(value % size)};
    value -= mod;
    return static_cast<T>(mod == T{0} ? value : value + size);
}

int user_id, pad_handle, audio_out_handle;
AvPlayerHandle av_player_handle;

// Dimensions
#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080
#define FRAME_DEPTH 4

static std::unordered_map<void*, std::size_t> g_flexmap;
static std::mutex g_flexmap_mutex;

void* av_allocate(void* handle, u32 alignment, u32 size) {
    LOG_INFO("called, size: {:#x}, alignment: {:#x}", size, alignment);
    u64 out = 0;
    const std::size_t mapped_size = AlignUp(static_cast<u64>(size), 16_KB);
    s32 ret = sceKernelMapFlexibleMemory(&out, mapped_size,
                                         MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite, 0);
    if (ret != 0) {
        LOG_ERROR("sceKernelMapFlexibleMemory failed: {:#x}", ret);
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lk(g_flexmap_mutex);
        g_flexmap[(void*)out] = mapped_size;
    }
    return (void*)out;
}

void av_deallocate(void* handle, void* memory) {
    LOG_INFO("called");
    if (!memory)
        return;
    std::size_t mapped_size = 0;
    {
        std::lock_guard<std::mutex> lk(g_flexmap_mutex);
        auto it = g_flexmap.find(memory);
        if (it != g_flexmap.end()) {
            mapped_size = it->second;
            g_flexmap.erase(it);
        } else {
            LOG_WARNING("av_deallocate: unknown pointer, falling back to 16_KB");
            mapped_size = 16_KB;
        }
    }
    sceKernelReleaseFlexibleMemory(memory, mapped_size);
    return;
}

void* av_allocate_texture(void* handle, u32 alignment, u32 size) {
    // LOG_INFO("called, size: {:#x}, alignment: {:#x}", size, alignment);
    u64 out = 0;
    const std::size_t mapped_size = AlignUp(static_cast<u64>(size), 16_KB);
    s32 ret = sceKernelMapFlexibleMemory(&out, mapped_size,
                                         MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite, 0);
    if (ret != 0) {
        LOG_ERROR("sceKernelMapFlexibleMemory (texture) failed: {:#x}", ret);
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lk(g_flexmap_mutex);
        g_flexmap[(void*)out] = mapped_size;
    }
    LOG_DEBUG("Allocated {:#x} bytes of memory to {}", mapped_size, (void*)out);
    return (void*)out;
}

void av_deallocate_texture(void* handle, void* memory) {
    // LOG_INFO("called");
    if (!memory)
        return;
    std::size_t mapped_size = 0;
    {
        std::lock_guard<std::mutex> lk(g_flexmap_mutex);
        auto it = g_flexmap.find(memory);
        if (it != g_flexmap.end()) {
            mapped_size = it->second;
            g_flexmap.erase(it);
        } else {
            LOG_WARNING("av_deallocate_texture: unknown pointer, falling back to 16_KB");
            mapped_size = 16_KB;
        }
    }
    sceKernelReleaseFlexibleMemory(memory, mapped_size);
    LOG_DEBUG("Released {:#x} bytes of memory from {}", mapped_size, memory);
    return;
}

void play_video_file(char const* path) {
    LOG_INFO("Playing {}", path);
    u32 ret;

    LOG_INFO("Initializing renderer");
    int frameID = 0;
    auto scene = new Scene2D(FRAME_WIDTH, FRAME_HEIGHT, FRAME_DEPTH);
    ASSERT_MSG(scene->Init(0xC000000, 2), "Failed to initialize 2D scene");

    if (!av_player_handle) {
        LOG_ERROR("sceAvPlayerInit returned an error.");
        return;
    }
    ret = sceAvPlayerAddSource(av_player_handle, path);
    if (ret) {
        LOG_ERROR("sceAvPlayerAddSource returned {:#x}", ret);
        return;
    }

    AvPlayerFrameInfo frame{};
    std::thread audio_thread = std::thread{[&]() {
        AvPlayerFrameInfo audio_data{};
        while (sceAvPlayerIsActive(av_player_handle)) {
            if (sceAvPlayerGetAudioData(av_player_handle, &audio_data)) {
                // LOG_INFO("audio size: {}, freq: {}", audio_data.details.audio.size,
                //          audio_data.details.audio.sample_rate);
                sceAudioOutOutput(audio_out_handle, audio_data.p_data);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        LOG_INFO("Exited the audio loop.");
    }};
    audio_thread.detach();
    LOG_INFO("Entering draw loop...");
    while (sceAvPlayerIsActive(av_player_handle)) {
        scene->FrameBufferClear();
        OrbisPadData pdata;
        scePadReadState(pad_handle, &pdata);
        if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE) != 0) {
            sceSystemServiceLoadExec("EXIT", nullptr);
        }
        // LOG_INFO("frame");
        if (!sceAvPlayerGetVideoData(av_player_handle, &frame)) {
            // LOG_INFO("sceAvPlayerGetVideoData failed"); // next frame not ready
            // Copy previous framebuffer to current one
            auto* dst =
                reinterpret_cast<uint32_t*>(scene->frameBuffers[scene->activeFrameBufferIdx]);
            auto* prev =
                reinterpret_cast<uint32_t*>(scene->frameBuffers[1 - scene->activeFrameBufferIdx]);
            std::memcpy(dst, prev, scene->width * scene->height * sizeof(uint32_t));
        } else {
            // LOG_INFO("sceAvPlayerGetVideoData succeeded (w: {}, h: {})",
            // frame.details.video.width,
            //          frame.details.video.height);
            auto* src = reinterpret_cast<const uint8_t*>(frame.p_data);
            auto* dst =
                reinterpret_cast<uint32_t*>(scene->frameBuffers[scene->activeFrameBufferIdx]);

            const int w = frame.details.video.width;
            const int h = frame.details.video.height;
            const u32 y_size = w * h;

            // fixed-point integer coefficients (scaled by 1024)
            constexpr int cR_V = 1436; // 1.4075 * 1024
            constexpr int cG_U = 352;  // 0.3455 * 1024
            constexpr int cG_V = 735;  // 0.7169 * 1024
            constexpr int cB_U = 1821; // 1.7790 * 1024

            for (int y = 0; y < h; ++y) {
                const int y_offset = y * w;
                const int uv_offset = y_size + (y / 2) * w;

                for (int x = 0; x < w; ++x) {
                    const int uv_index = uv_offset + (x & ~1);

                    // NV12 (swap U/V if NV21)
                    const int U = src[uv_index + 0] - 128;
                    const int V = src[uv_index + 1] - 128;
                    const int Y = src[y_offset + x];

                    // fixed-point YUV → RGB
                    int r = (Y << 10) + cR_V * V;
                    int g = (Y << 10) - cG_U * U - cG_V * V;
                    int b = (Y << 10) + cB_U * U;

                    // convert back to 8-bit and clamp
                    r = std::clamp(r >> 10, 0, 255);
                    g = std::clamp(g >> 10, 0, 255);
                    b = std::clamp(b >> 10, 0, 255);

                    dst[y_offset + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
                }
            }
        }

        // Submit the frame buffer
        scene->SubmitFlip(frameID);
        scene->FrameWait(frameID);

        // Swap to the next buffer
        scene->FrameBufferSwap();
        frameID++;
    }
    LOG_INFO("Exited the draw loop.");

    sceAvPlayerStop(av_player_handle);
    sceAvPlayerClose(av_player_handle);
}

void init_libs() {
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_AV_PLAYER);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT);
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    scePadInit();
    sceAudioOutInit();
    pad_handle = scePadOpen(user_id, 0, 0, 0);
    AvPlayerInitData init = AvPlayerInitData{0};
    {
        init.memory_replacement.object_ptr = nullptr;
        init.memory_replacement.allocate = &av_allocate;
        init.memory_replacement.deallocate = &av_deallocate;
        init.memory_replacement.allocate_texture = &av_allocate_texture;
        init.memory_replacement.deallocate_texture = &av_deallocate_texture;
        init.debug_level = AvPlayerDebuglevels::Warnings;
        init.base_priority = 700;
        init.num_output_video_framebuffers = 2;
        init.auto_start = true;
        init.default_language = "";
    }
    av_player_handle = sceAvPlayerInit(&init);
    audio_out_handle =
        sceAudioOutOpen(255, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                        /* saples to submit per call */ 1024,
                        /* sample rate */ 48000, /* S16_MONO = 0, S16_STEREO = 1 */ 1);
    if (audio_out_handle < 0) {
        LOG_ERROR("sceAudioOutOpen returned {:#x}", (u32)audio_out_handle);
    }
}

int main(int argc, char** argv) {
    init_libs();
    setvbuf(stdout, NULL, _IONBF, 0);

    OrbisKernelStat s;
    if (argc > 1) {
        play_video_file(argv[1]);
    } else if (sceKernelStat("/data/homebrew/video.mp4", &s) == 0) {
        play_video_file("/data/homebrew/video.mp4");
    } else if (sceKernelStat("/app0/video.mp4", &s) == 0) {
        play_video_file("/app0/video.mp4");
    } else {
        play_video_file("/app0/video_short.mp4");
    }

    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
