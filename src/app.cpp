#include "app.h"

#include <map>

App::App() {
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

void App::Run() {
    use_font = false;

    InitGraphics();
    InitFont();

    while (HandleControllerInput()) {
        FrameStart();
        DrawDemo();
        FrameEnd();
    }
}

void App::InitGraphics() {
    scene = new Scene2D(1920, 1080, 4);
    ASSERT_MSG(scene->Init(0xC000000, 2), "Failed to initialize 2D scene");
}

void App::InitFont() {
    if (!use_font) {
        return;
    }
    ASSERT_OK(scene->InitFontLib());
    std::string font_path =
        fmt::format("/{}/common/font/DFHEI5-SONY.ttf", sceKernelGetFsSandboxRandomWord());
    ASSERT_MSG(scene->InitFont(&font, font_path.c_str(), 80) && font != nullptr,
               "Failed to init font");
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
        if (!use_font) {
            use_font = true;
            InitFont();
        }
    }
    return true;
}

void App::FrameStart() {
    scene->FrameBufferClear();
}

void App::FrameEnd() {
    scene->SubmitFlip();
    scene->FrameWait();
    scene->FrameBufferSwap();
}


void App::DrawDemo() {
    scene->DrawRectangle(100, 100, 200, 200, {0, 0, 0});
    scene->DrawRectangle(120, 120, 160, 160, {255, 128, 128});
    scene->DrawRectangle(140, 140, 120, 120, {0, 0, 0});
    if (!use_font) {
        return;
    }
    scene->DrawText("Hello, Screen!", font, 400, 220, {50, 50, 50}, {0, 0, 255});
}