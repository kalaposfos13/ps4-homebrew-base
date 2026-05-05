#include "app.h"
#include "orbis_hmd.h"

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
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_HMD);
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    scePadInit();
    ASSERT_NO_ERROR(pad_handle = scePadOpen(user_id, 0, 0, 0));
    LOG_INFO("userid: {}, pad handle: {:x}", user_id, pad_handle);

    OrbisHmdInitializeParam hp{};
    sceHmdInitialize(&hp);
    OrbisHmdDeviceInformation hmd_device_info{};
    ASSERT_OK(sceHmdGetDeviceInformation(&hmd_device_info));
    LOG_INFO("Hmd status: {}", hmd_device_info.status);
    if (hmd_device_info.status != 0 /* ready */) {
        LOG_NOTIFICATION("No PSVR connected (status: {}).", hmd_device_info.status);
        sceSystemServiceLoadExec("exit", nullptr);
    }
}

App::~App() {
    LOG_INFO("App stopped.");
    sceHmdTerminate();
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
    const auto is_button_down = [this](OrbisPadButton b){
        return (pdata.buttons & b) != 0;
    };
    if (is_button_pressed(OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE)) {
        return false;
    }
    if (is_button_pressed(OrbisPadButton::ORBIS_PAD_BUTTON_SQUARE)) {
        state.eye = 1 - state.eye;
    }
    if (is_button_pressed(OrbisPadButton::ORBIS_PAD_BUTTON_TRIANGLE)) {
        scale = 1.2f;
    }
    if (is_button_down(OrbisPadButton::ORBIS_PAD_BUTTON_L2)) {
        scale *= 1/1.02f;
    }
    if (is_button_down(OrbisPadButton::ORBIS_PAD_BUTTON_R2)) {
        scale *= 1.02f;
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

    int screenW = renderer.scene->width;
    int screenH = renderer.scene->height;
    int halfW = screenW / 2;
    int cropX = 0;
    int cropY = 0;

    renderer.DrawImage(right_eye, 
        0, 0, 
        halfW, screenH, 
        cropX, cropX, cropY, cropY, scale);

    renderer.DrawImage(left_eye, 
        halfW, 0,
        halfW, screenH,
        cropX, cropX, cropY, cropY, scale);
}

void App::DrawPlaceholderCameraImage() {
    renderer.scene->DrawRectangle(0, 0, 1280, 800, {0, 0, 0});
}