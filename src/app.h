#pragma once

#include "assert.h"
#include "graphics.h"
#include "logging.h"
#include "types.h"
#include "camera.h"

#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <cstdlib>
#include "renderer.h"
#include "mic.h"
#include "pad.h"

struct AppState {
    s32 eye = 0;
};

class App {
public:
    App();
    ~App();

    void Run();

    void DrawDemo();

    bool HandleControllerInput();

    bool use_font = false;

    s32 user_id{};
    Pad pad{};

    OrbisPadData pdata{};

    Microphone mic{};
    Renderer renderer{};
    AppState state{};
};
