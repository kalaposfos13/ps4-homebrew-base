#include "assert.h"
#include "mic.h"

#include "orbis/Sysmodule.h"

#include <thread>

extern "C" int pthread_rename_np(pthread_t thread, const char* name);

Microphone::Microphone() {
}


void Microphone::Init(s32 uid) {
    ASSERT_NO_ERROR(handle = sceAudioInHqOpen(uid, 1, 0, 128, 48000, 2));
    // 5 seconds of 48khz stereo data is a sensible baseline imo
    recorded_data.resize(48000 * 5 * 2);

}

Microphone::~Microphone() {
    EndRecording();
    sceKernelUsleep(3000);
    sceAudioInClose(handle);
}

void Microphone::BeginRecording() {
    LOG_INFO("called");
    ASSERT(!is_recording);
    recorded_data.clear();
    is_recording = true;
    LOG_INFO("starting reader thread");
    std::thread reader_thread{[&](){
        LOG_INFO("hi");
        pthread_rename_np(pthread_self(), "mic reader thread");
        int buf_index = 0;
        while(is_recording) {
            auto b = input_bufs[buf_index];
            s32 written = sceAudioInInput(handle, b);
            ASSERT_NO_ERROR(written);
            std::copy(b, b + written, std::back_inserter(recorded_data));
            buf_index ^= 1;
        }
        ASSERT_NO_ERROR(sceAudioInInput(handle, nullptr));
    }};
    reader_thread.detach();
    LOG_INFO("reader thread started");
}

void Microphone::EndRecording() {
    is_recording = false;
}

u8* Microphone::ReadPacket() {
    return nullptr;
}
