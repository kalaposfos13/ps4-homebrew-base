#include "app.h"

#include <map>
#include <utility>

void App::Run() {
    while (HandleInput()) {
        renderer.BeginFrame();
        DrawDemo();
        renderer.EndFrame();
    }
}

App::App() {
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    LOG_INFO("userid: {}", user_id);
    pad.Init(user_id);
}

App::~App() {
    LOG_INFO("App stopped.");
    sceSystemServiceLoadExec("EXIT", nullptr);
}

bool App::HandleInput() {
    pad.Update();
    if (pad.IsPressed(OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE)) {
        return false;
    }
    return true;
}

void App::DrawDemo() {
    renderer.scene->DrawRectangle(100, 100, 200, 200, {0, 0, 0});
    renderer.scene->DrawRectangle(120, 120, 160, 160, {255, 128, 128});
    renderer.scene->DrawRectangle(140, 140, 120, 120, {0, 0, 0});
    renderer.scene->DrawText("Hello, Screen!", renderer.font, 400, 220, {50, 50, 50}, {0, 0, 255});
}
