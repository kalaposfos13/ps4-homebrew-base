#pragma once

#include <orbis/Pad.h>
#include "types.h"

class Input {
public:
    bool Init(s32 user_id);
    bool Update();

    const OrbisPadData& GetState() const { return pdata; }

private:
    s32 handle{};
    OrbisPadData pdata{};
};