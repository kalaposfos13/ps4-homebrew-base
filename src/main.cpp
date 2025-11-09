#include <cstring>
#include <string>
#include <vector>
#include <fcntl.h>
#include "orbis/AudioOut.h"
#include "orbis/Pad.h"
#include "orbis/SystemService.h"
#include "orbis/UserService.h"
#include "orbis/libkernel.h"

#include "av_player.h"
#include "graphics.h"

#include "logging.h"

#include <thread>
#include "assert.h"

int user_id, pad_handle, audio_out_handle;
AvPlayerHandle av_player_handle;

void render_video_frame(Scene2D* scene, const AvPlayerFrameInfo& frame) {
    const uint8_t* src = reinterpret_cast<const uint8_t*>(frame.p_data);
    uint32_t* dst = reinterpret_cast<uint32_t*>(scene->frameBuffers[scene->activeFrameBufferIdx]);

    // fixed-point BT.601 limited-range YUV → RGB (scaled by 1024)
    constexpr int cY = 1192;   // 1.164 * 1024
    constexpr int cR_V = 1634; // 1.596 * 1024
    constexpr int cG_U = 400;  // 0.392 * 1024
    constexpr int cG_V = 833;  // 0.813 * 1024
    constexpr int cB_U = 2066; // 2.017 * 1024

    const int sw = frame.details.video.width;
    const int sh = frame.details.video.height;
    const int dw = scene->width;
    const int dh = scene->height;
    const u32 y_size = sw * sh;

    for (int y = 0; y < dh; ++y) {
        const int sy = y * sh / dh;
        const int y_offset = sy * sw;
        const int uv_offset = y_size + (sy / 2) * sw;

        for (int x = 0; x < dw; ++x) {
            const int sx = x * sw / dw;
            const int uv_index = uv_offset + (sx & ~1);

            const int U = src[uv_index + 0] - 128;
            const int V = src[uv_index + 1] - 128;
            const int Y = src[y_offset + sx];

            int yv = std::max(0, Y - 16);

            int r = (cY * yv + cR_V * V) >> 10;
            int g = (cY * yv - cG_U * U - cG_V * V) >> 10;
            int b = (cY * yv + cB_U * U) >> 10;

            // clamp to 0–255
            r = std::clamp(r, 0, 255);
            g = std::clamp(g, 0, 255);
            b = std::clamp(b, 0, 255);

            dst[y * dw + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

void play_video_file(char const* path) {
    LOG_INFO("Playing {}", path);
    u32 ret;

    LOG_INFO("Initializing renderer");
    int frameID = 0;
    auto scene = new Scene2D(1920, 1080, 4);
    ASSERT_MSG(scene->Init(0xC000000, 2), "Failed to initialize 2D scene");
    FT_Face font = nullptr;
    ASSERT_MSG(scene->InitFont(&font, "/data/homebrew/assets/Monocraft.ttf", 20) && font != nullptr,
               "Failed to init font");

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
            // std::swap(scene->frameBuffers[scene->activeFrameBufferIdx], scene->frameBuffers[1 -
            // scene->activeFrameBufferIdx]);
        } else {
            // LOG_INFO("sceAvPlayerGetVideoData succeeded (w: {}, h: {})",
            // frame.details.video.width,
            //          frame.details.video.height);
            render_video_frame(scene, frame);
        }
        char const* text = R"(We're no strangers to love
You know the rules and so do I
A full commitment's what I'm thinkin' of
You wouldn't get this from any other guy)";
        scene->DrawText(text, font, 50, 300, {255, 255, 255}, {255, 255, 255});

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
    std::vector<char const*> video_paths = {
        "/data/homebrew/video.mp4",
        "/app0/video.mp4",
        "/app0/video_short.mp4",
    };
    if (argc > 1 && sceKernelStat(argv[1], &s) == 0) {
        play_video_file(argv[1]);
    } else {
        for (auto const& path : video_paths) {
            if (sceKernelStat(path, &s) == 0) {
                play_video_file(path);
            }
        }
    }

    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
