#include "app.h"

#include <map>
#include <utility>

void App::Run() {
    while (HandleControllerInput()) {
        renderer.BeginFrame();
        renderer.EndFrame();
    }
}

App::App() {
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_LIBIME);
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    LOG_INFO("userid: {:#x}", user_id);
    pad.Init(user_id);
}

App::~App() {
    LOG_INFO("App stopped.");
    sceSystemServiceLoadExec("EXIT", nullptr);
}

bool App::HandleControllerInput() {
    pad.Update();
    if (pad.IsPressed(OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE)) {
        return false;
    }
    if (pad.IsPressed(OrbisPadButton::ORBIS_PAD_BUTTON_SQUARE)) {
        static bool on_state = false;
        on_state ^= true;
        if (on_state) {
            mic.BeginRecording();
        } else {
            mic.EndRecording();
        }
    }
    return true;
}

void App::DrawDemo() {
    renderer.scene->DrawRectangle(100, 100, 200, 200, {0, 0, 0});
    renderer.scene->DrawRectangle(120, 120, 160, 160, {255, 128, 128});
    renderer.scene->DrawRectangle(140, 140, 120, 120, {0, 0, 0});
    if (!use_font) {
        return;
    }
    renderer.scene->DrawText("Hello, Screen!", renderer.font, 400, 220, {50, 50, 50}, {0, 0, 255});
}
