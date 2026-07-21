#include "assert.h"
#include "pad.h"

#include <unordered_map>

void Pad::Init(s32 user_id) {
    scePadInit();
    ASSERT_NO_ERROR(pad_handle = scePadOpen(user_id, 0, 0, 0));
    LOG_INFO("Pad handle: {:x}", pad_handle);
}

Pad::~Pad() {
    scePadClose(pad_handle);
}

void Pad::Update() {
    scePadReadState(pad_handle, &pdata);
}

bool Pad::IsHeld(OrbisPadButton b) {
    return (pdata.buttons & b) != 0;
}

bool Pad::IsPressed(OrbisPadButton b) {
    static std::unordered_map<OrbisPadButton, bool> btn_pressed{};

    if (!btn_pressed.contains(b)) {
        btn_pressed[b] = false;
    }
    if (IsHeld(b)) {
        if (!btn_pressed[b]) {
            btn_pressed[b] = true;
            return true;
        }
    } else {
        btn_pressed[b] = false;
    }
    return false;
}
