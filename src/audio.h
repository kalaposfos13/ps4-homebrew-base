#pragma once

#include "types.h"

class Audio {
public:
    Audio();
    ~Audio();
    void Init(s32 uid);
    void PlaySound(s16* data, s32 len);
    s16* playing_buf = nullptr;
    s32 playing_len = 0;
    s32 playing_offset = 0;

    s32 handle;
};