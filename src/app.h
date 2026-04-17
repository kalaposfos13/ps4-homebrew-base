#pragma once

#include "assert.h"
#include "graphics.h"
#include "logging.h"
#include "types.h"

#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <cstdlib>

enum Status : u32 {
    Tracking,
    NotTracking,
    Calibrating,
    Error,
};

struct AppState {
};

class App {
public:
    App();
    ~App();

    void Run();

    void InitGraphics();
    void InitFont();

    void FrameStart();
    void FrameEnd();

    void DrawDemo();

    bool HandleControllerInput();

    bool use_font = false;

    s32 user_id{};
    s32 pad_handle{};

    OrbisPadData pdata{};

    Scene2D* scene{};
    AppState state{};
    FT_Face font{};
};
