#include "audio.h"
#include "logging.h"
#include "orbis/AudioOut.h"

#include <mutex>
#include <thread>

extern "C" int pthread_set_name_np(pthread_t thread, const char* name);

Audio::Audio() {}

Audio::~Audio() {}

static std::mutex m{};

void Audio::Init(s32 uid) {
    sceAudioOutInit();
    handle =
        sceAudioOutOpen(uid, OrbisAudioOutPort::ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, 0);
    auto playback_thread = std::thread([this]() {
        pthread_set_name_np(pthread_self(), "audio out thread");
        while (true) {
            bool valid = false;
            {
                std::scoped_lock l{m};
                valid = playing_buf == nullptr || playing_offset >= playing_len;
            }
            if (valid) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            {
                auto advance = sceAudioOutOutput(handle, playing_buf + playing_offset);
                std::scoped_lock l{m};
                // LOG_INFO("{}", advance);
                playing_offset += advance;
            }
        }
    });
    playback_thread.detach();
}

void Audio::PlaySound(s16* data, s32 len) {
    std::scoped_lock l{m};
    playing_buf = data;
    playing_len = len;
    playing_offset = 0;
}
