#include "app.h"

#include <map>
#include <utility>

void App::Run() {
    while (HandleControllerInput()) {
        camera.Update();
        renderer.BeginFrame();
        DrawCameraImage();
        renderer.EndFrame();
    }
}

App::App() {
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_LIBIME);
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    scePadInit();
    ASSERT_NO_ERROR(pad_handle = scePadOpen(user_id, 0, 0, 0));
    LOG_INFO("userid: {}, pad handle: {:x}", user_id, pad_handle);
}

App::~App() {
    LOG_INFO("App stopped.");
    sceSystemServiceLoadExec("EXIT", nullptr);
}

bool App::HandleControllerInput() {
    scePadReadState(pad_handle, &pdata);
    static std::map<OrbisPadButton, bool> btn_pressed{};
    auto is_button_pressed = [&, this](OrbisPadButton b) {
        if (!btn_pressed.contains(b)) {
            btn_pressed[b] = false;
        }
        if ((pdata.buttons & b) != 0) {
            if (!btn_pressed[b]) {
                btn_pressed[b] = true;
                return true;
            }
        } else {
            btn_pressed[b] = false;
        }
        return false;
    };
    if (is_button_pressed(OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE)) {
        return false;
    }
    if (is_button_pressed(OrbisPadButton::ORBIS_PAD_BUTTON_SQUARE)) {
        state.eye = 1 - state.eye;
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

void App::DrawCameraImage() {
    if (camera.handle == 0) {
        DrawPlaceholderCameraImage();
        return;
    }
    static Image left_eye{}, right_eye{};
    camera.RenderEyeToImage(0, 1280, 800, left_eye);
    camera.RenderEyeToImage(1, 1280, 800, right_eye);
    renderer.DrawImage(left_eye, 0, 0);
    renderer.DrawImage(right_eye, 640, 280);
}

void App::DrawPlaceholderCameraImage() {
    renderer.scene->DrawRectangle(0, 0, 1280, 800, {0, 0, 0});
}