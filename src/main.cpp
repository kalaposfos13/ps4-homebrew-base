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
    LOG_INFO("called, size: {:#x}, alignment: {:#x}", size, alignment);
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
    return (void*)out;
}

void av_deallocate_texture(void* handle, void* memory) {
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
            LOG_WARNING("av_deallocate_texture: unknown pointer, falling back to 16_KB");
            mapped_size = 16_KB;
        }
    }
    sceKernelReleaseFlexibleMemory(memory, mapped_size);
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
    AvPlayerFrameInfo audio_data{};
    LOG_INFO("Entering draw loop...");
    while (sceAvPlayerIsActive(av_player_handle)) {
        scene->FrameBufferClear();
        OrbisPadData pdata;
        scePadReadState(pad_handle, &pdata);
        if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE) != 0) {
            sceSystemServiceLoadExec("EXIT", nullptr);
        }
        // LOG_INFO("frame");
        if (sceAvPlayerGetAudioData(av_player_handle, &audio_data)) {
            LOG_INFO("audio size: {}, freq: {}", audio_data.details.audio.size,
                     audio_data.details.audio.sample_rate);
            u16 sbuf[2048]{0};
            for (int segment = 0; segment < 1; segment++) {
                auto* vsbuf = reinterpret_cast<u16*>(audio_data.p_data + segment * 2048 * 2);
                for (int i = 0; i < 2048; i++) {
                    sbuf[i] = vsbuf[u64(
                        (float)(i) * (float)((audio_data.details.audio.sample_rate / 48000.0f)))];
                }
                sceAudioOutOutput(audio_out_handle, sbuf);
            }
            // sceAudioOutOutput(audio_out_handle, nullptr); // flush
        }
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

            for (int j = 0; j < frame.details.video.height; ++j) {
                for (int i = 0; i < frame.details.video.width; ++i) {
                    uint8_t luminance = src[j * frame.details.video.width + i];
                    uint32_t color = 0xFF000000u | (luminance << 16) | (luminance << 8) | luminance;
                    dst[j * scene->width + i] = color;
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

    sceAvPlayerClose(av_player_handle);
    sceAvPlayerStop(av_player_handle);
}

void init_libs() {
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_AV_PLAYER);
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
    audio_out_handle = sceAudioOutOpen(user_id, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                                       /* saples to submit per call */ 2048,
                                       /* sample rate */ 48000, 0 /* S16_MONO */);
}

int main(int argc, char** argv) {
    init_libs();
    setvbuf(stdout, NULL, _IONBF, 0);

    OrbisKernelStat s;
    if (argc > 1) {
        play_video_file(argv[1]);
    } else if (sceKernelStat("/data/homebrew/video.mp4", &s) == 0) {
        play_video_file("/data/homebrew/video.mp4");
        // } else if (sceKernelStat("/app0/video.mp4", &s) == 0) {
        //     play_video_file("/app0/video.mp4");
    } else {
        play_video_file("/app0/video_short.mp4");
    }

    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
