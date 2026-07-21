#pragma once

#include <orbis/Pad.h>
#include "types.h"

class Pad {
public:
    s32 pad_handle{};
    OrbisPadData pdata{};

    Pad() = default;
    ~Pad();
    void Init(s32 user_id);

    void Update();

    bool IsHeld(OrbisPadButton b);
    bool IsPressed(OrbisPadButton b);
};