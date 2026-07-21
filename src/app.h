#pragma once

#include "assert.h"
#include "camera.h"
#include "graphics.h"
#include "logging.h"
#include "pad.h"
#include "types.h"

#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <cstdlib>
#include "renderer.h"

class App {
public:
    App();
    ~App();

    void Run();

    void DrawDemo();

    bool HandleInput();

    s32 user_id{};
    Pad pad{};
    s32 kb_handle{};

    Renderer renderer{};
};
