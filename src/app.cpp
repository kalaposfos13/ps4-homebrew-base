#include "app.h"

#include <filesystem>
#include <fstream>
#include <map>

const char* dump_path[2] = {"/data/homebrew/frame_dump1.bin", "/data/homebrew/frame_dump2.bin"};

static inline uint32_t YUVtoRGBA(uint8_t y, uint8_t u, uint8_t v) {
    int c = (int)y - 16;
    int d = (int)u - 128;
    int e = (int)v - 128;

    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;

    if (r < 0)
        r = 0;
    else if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    else if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    else if (b > 255)
        b = 255;

    return 0x80000000u | (r << 16) | (g << 8) | b;
}
void DrawYUV422Frame(Scene2D* scene, void* yuvBuffer, int width, int height) {
    u8* src = static_cast<u8*>(yuvBuffer);
    u32* dst = reinterpret_cast<u32*>(scene->frameBuffers[scene->activeFrameBufferIdx]);

    for (int y = 0; y < height; y++) {
        u32* row = dst + y * scene->width;
        int srcRow = y * width * 2;

        for (int x = 0; x < width; x += 2) {
            int idx = srcRow + x * 2;

            u16 y0 = src[idx + 0];
            u16 u = src[idx + 1];
            u16 y1 = src[idx + 2];
            u16 v = src[idx + 3];

            row[x + 0] = YUVtoRGBA(y0, u, v);
            row[x + 1] = YUVtoRGBA(y1, u, v);
        }
    }
}
void DrawRAW16Frame(Scene2D* scene, void* rawBuffer, int width, int height) {
    const u16* src = static_cast<const u16*>(rawBuffer);
    u32* dst = reinterpret_cast<u32*>(scene->frameBuffers[scene->activeFrameBufferIdx]);

    for (int y = 0; y < height - 1; y++) {
        for (int x = 0; x < width - 1; x++) {
            int idx = y * width + x;

            bool evenRow = (y % 2) == 0;
            bool evenCol = (x % 2) == 0;

            u16 R = 0, G = 0, B = 0;

            if (evenRow && evenCol) {
                // B
                B = src[idx];
                G = src[idx + 1];
                R = src[idx + width + 1];
            } else if (evenRow && !evenCol) {
                // G (blue row)
                G = src[idx];
                B = src[idx - 1];
                R = src[idx + width];
            } else if (!evenRow && evenCol) {
                // G (red row)
                G = src[idx];
                R = src[idx + 1];
                B = src[idx - width];
            } else if (!evenRow && !evenCol) {
                // R
                R = src[idx];
                G = src[idx - 1];
                B = src[idx - width - 1];
            }

            constexpr u16 WHITE = 4095;

            u8 r = std::min(WHITE, R) >> 4;
            u8 g = std::min(WHITE, G) >> 4;
            u8 b = std::min(WHITE, B) >> 4;

            dst[y * scene->width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

enum MemoryProt : u32 {
    NoAccess = 0,
    CpuRead = 1,
    CpuWrite = 2,
    CpuReadWrite = 3,
    CpuExec = 4,
    GpuRead = 16,
    GpuWrite = 32,
    GpuReadWrite = 48,
};
template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, std::size_t size) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    auto mod{static_cast<T>(value % size)};
    value -= mod;
    return static_cast<T>(mod == T{0} ? value : value + size);
}
static VAddr alloc_memory(size_t size, size_t alignment, u32 memType, u32 prot) {
    size = AlignUp(size, 16_KB);
    ASSERT(alignment == AlignUp(alignment, 16_KB));

    off_t phys = 0;
    ASSERT_OK(sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), size, alignment,
                                            memType, &phys));

    void* virt = nullptr;
    ASSERT_OK(sceKernelMapDirectMemory(&virt, size, prot, 0, phys, alignment));

    return VAddr(virt);
}

App::App() {
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_MOVE);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_MOVE_TRACKER);

    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    scePadInit();
    ASSERT_NO_ERROR(pad_handle = scePadOpen(user_id, 0, 0, 0));
    LOG_INFO("userid: {}, pad handle: {:x}", user_id, pad_handle);

    int frameID = 0;
    scene = new Scene2D(1920, 1080, 4);
    ASSERT_MSG(scene->Init(0xC000000, 2), "Failed to initialize 2D scene");

#ifdef GRAPHICS_USES_FONT
    std::string font_path =
        fmt::format("/{}/common/font/DFHEI5-SONY.ttf", sceKernelGetFsSandboxRandomWord());
    ASSERT_MSG(scene->InitFont(&font, font_path.c_str(), 40) && font != nullptr,
               "Failed to init font");
#endif
}

App::~App() {
    sceCameraStop(camera_handle);
    sceCameraClose(camera_handle);
    LOG_INFO("App stopped.");
    sceSystemServiceLoadExec("EXIT", nullptr);
}

bool App::InitCamera() {
    if (sceCameraIsAttached(0) != 1) {
        return false;
    }

    ASSERT_NO_ERROR(camera_handle =
                        sceCameraOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, 0, 0, nullptr));
    LOG_INFO("PlayStation Camera connected (handle: {})", camera_handle);

    OrbisCameraConfig cconfig{};
    cconfig.size_this = sizeof(OrbisCameraConfig);
    cconfig.config_type = ORBIS_CAMERA_CONFIG_TYPE4;
    ASSERT_OK(sceCameraSetConfig(camera_handle, &cconfig));

    OrbisCameraStartParameter cstart_param{};
    cstart_param.size_this = sizeof(OrbisCameraStartParameter);
    cstart_param.format_level[0] = ORBIS_CAMERA_FRAME_FORMAT_LEVEL0;
    cstart_param.format_level[1] = ORBIS_CAMERA_FRAME_FORMAT_LEVEL0;
    ASSERT_OK(sceCameraStart(camera_handle, &cstart_param));

    OrbisCameraVideoSyncParameter cvsync_param{};
    cvsync_param.size_this = sizeof(OrbisCameraVideoSyncParameter);
    cvsync_param.video_sync_mode = 1;
    ASSERT_OK(sceCameraSetVideoSync(camera_handle, &cvsync_param));
    LOG_INFO("PlayStation Camera set up and started.");

    frame_data.size_this = (sizeof(OrbisCameraFrameData));
    frame_data.read_mode = 0;
    frame_data.meta.exposureGain[0].exposureControl = 0;
    frame_data.meta.exposureGain[1].exposureControl = 0;

    sceCameraGetExposureGain(camera_handle, 1, &exposuregain, nullptr);

    if (use_dumped_frame) {
        dumped_frame_buf[0] = (void*)alloc_memory(1280 * 800 * 2, 0, ORBIS_KERNEL_WB_ONION,
                                                  MemoryProt::CpuReadWrite | MemoryProt::GpuRead);
        std::ifstream is1(dump_path[0], std::ios::binary);
        is1.read(reinterpret_cast<char*>(dumped_frame_buf[0]), 1280 * 800 * 2);

        dumped_frame_buf[1] = (void*)alloc_memory(1280 * 800 * 2, 0, ORBIS_KERNEL_WB_ONION,
                                                  MemoryProt::CpuReadWrite | MemoryProt::GpuRead);
        std::ifstream is2(dump_path[1], std::ios::binary);
        is2.read(reinterpret_cast<char*>(dumped_frame_buf[1]), 1280 * 800 * 2);
    }
    return true;
}

void App::InitMoveTracker() {
    if (!use_tracking) {
        return;
    }

    LOG_INFO("Setting up move tracker");

    mt_images[0].exposure = exposuregain.exposure;
    mt_images[0].gain = exposuregain.gain;
    mt_images[1].exposure = exposuregain.exposure;
    mt_images[1].gain = exposuregain.gain;

    mt_controllers[0].handle = move_handle;
    mt_controllers[0].data = &m_data[0];
    mt_controllers[0].num = 1;
    mt_controllers[1].handle = -1;
    mt_controllers[1].data = &m_data[1];
    mt_controllers[1].num = 0;
    mt_controllers[2].handle = -1;
    mt_controllers[2].data = &m_data[2];
    mt_controllers[2].num = 0;
    mt_controllers[3].handle = -1;
    mt_controllers[3].data = &m_data[3];
    mt_controllers[3].num = 0;

    s32 mt_o_s, mt_g_s;
    ASSERT_OK(sceMoveTrackerGetWorkingMemorySize(&mt_o_s, &mt_g_s));
    ASSERT(mt_o_s > 0 && mt_g_s > 0);
    LOG_INFO("needed onion size: {:#x}, needed garlic size: {:#x}", mt_o_s, mt_g_s);
    VAddr mt_onion = alloc_memory(mt_o_s, 0, ORBIS_KERNEL_WB_ONION,
                                  MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite);
    VAddr mt_garlic = alloc_memory(mt_g_s, 0, ORBIS_KERNEL_WC_GARLIC,
                                   MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite);
    ASSERT_OK(sceMoveTrackerInit(mt_onion, mt_garlic, 0, 1));
}

static int dump_next_camera_frame_id = -1;

bool App::UpdateCamera() {
    if (camera_handle == 0) {
        if (!InitCamera()) {
            return false;
        }
    }
    if (sceCameraGetFrameData(camera_handle, &frame_data) != ORBIS_OK) {
        sceKernelUsleep(10000);
        return false;
    }
    if (!sceCameraIsValidFrameData(camera_handle, &frame_data)) {
        return false;
    }
    if (dump_next_camera_frame_id >= 0) {
        LOG_NOTIFICATION("Dumping frame {}...", dump_next_camera_frame_id);
        std::ofstream os(dump_path[dump_next_camera_frame_id], std::ios::binary | std::ios::out);
        os.write(reinterpret_cast<char const*>(frame_data.frame_ptr_list[0][0]),
                 frame_data.frame_size[0][0]);
        dump_next_camera_frame_id = -1;
    }
    return true;
}

void App::UpdateMoveTracker() {
    if (!use_tracking) {
        return;
    }
    auto now = sceKernelGetProcessTime();
    mt_images[0].timestamp = now;
    mt_images[1].timestamp = now;
    mt_images[0].data = frame_data.frame_ptr_list[0][0];
    mt_images[1].data = frame_data.frame_ptr_list[1][0];
    ASSERT_OK(sceMoveTrackerCameraUpdate(mt_images, frame_data.meta.acceleration));
    ASSERT_OK(sceMoveTrackerControllersUpdate(mt_controllers));
    s32 get_state_status = 0;
    ASSERT_NO_ERROR(get_state_status = sceMoveTrackerGetState(move_handle, s64(-1), &mt_state));
    switch (get_state_status) {
    case 0:
        state.mt_status = Status::Tracking;
        break;
    case 3:
        state.mt_status = Status::Calibrating;
        break;
    default:
        LOG_WARNING("Status is {}", get_state_status);
        break;
    }
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
    if (is_button_pressed(OrbisPadButton::ORBIS_PAD_BUTTON_LEFT)) {
        dump_next_camera_frame_id = 0;
    }
    if (is_button_pressed(OrbisPadButton::ORBIS_PAD_BUTTON_RIGHT)) {
        dump_next_camera_frame_id = 1;
    }
    if (is_button_pressed(OrbisPadButton::ORBIS_PAD_BUTTON_L1)) {
        s32 r = rand() % 8;
        move_ball_colour.r = (r & 0b100) ? 255 : 0;
        move_ball_colour.g = (r & 0b010) ? 255 : 0;
        move_ball_colour.b = (r & 0b001) ? 255 : 0;
        sceMoveSetLightSphere(move_handle, move_ball_colour.r, move_ball_colour.g,
                              move_ball_colour.b);
    }
    sceMoveSetVibration(move_handle, pdata.analogButtons.l2);

    return true;
}

bool App::HandleMoveInput() {
    ASSERT_NO_ERROR(sceMoveReadStateLatest(move_handle, m_data));
    static std::map<OrbisMoveButtonDataOffset, bool> m_btn_pressed{};
    auto is_m_button_pressed = [&, this](OrbisMoveButtonDataOffset b) {
        if (!m_btn_pressed.contains(b)) {
            m_btn_pressed[b] = false;
        }
        if ((m_data[0].button_data.button_data & b) != 0) {
            if (!m_btn_pressed[b]) {
                m_btn_pressed[b] = true;
                return true;
            }
        } else {
            m_btn_pressed[b] = false;
        }
        return false;
    };
    if (is_m_button_pressed(OrbisMoveButtonDataOffset::Circle)) {
        return false;
    }

    return true;
}

void App::FrameStart() {
    scene->FrameBufferClear();
}

void App::FrameEnd() {
    scene->SubmitFlip(frame_id);
    scene->FrameWait(frame_id);
    scene->FrameBufferSwap();
    frame_id++;
}

void App::DrawCameraImage() {
    if (camera_handle == 0) {
        DrawPlaceholderCameraImage();
        return;
    }
    if (use_dumped_frame) {
        DrawRAW16Frame(scene, dumped_frame_buf[state.mt_status == Status::Calibrating ? 0 : 1],
                       1280, 800);
        return;
    }
    if (!frame_data.frame_ptr_list[state.eye][0]) {
        DrawPlaceholderCameraImage();
        return;
    }
    switch (frame_data.meta.format[state.eye][0]) {
    case ORBIS_CAMERA_FORMAT_YUV422:
        DrawYUV422Frame(scene, frame_data.frame_ptr_list[state.eye][0], 1280, 800);
        break;
    case ORBIS_CAMERA_FORMAT_RAW16:
        DrawRAW16Frame(scene, frame_data.frame_ptr_list[state.eye][0], 1280, 800);
        break;
    case ORBIS_CAMERA_FORMAT_RAW8:
    default:
        UNREACHABLE();
        break;
    }
}

void App::DrawPlaceholderCameraImage() {
        scene->DrawRectangle(0, 0, 1280, 800, {0, 0, 0});
}

void App::InitMove() {
    ASSERT_OK(sceMoveInit());
    LOG_CALL(move_handle = sceMoveOpen(user_id, /*standard*/ 0, 0));
    LOG_CALL(sceMoveSetLightSphere(move_handle, move_ball_colour.r, move_ball_colour.g, move_ball_colour.b));
}

void App::DrawMoveResult() {
    auto const& md = m_data[0];
    // LOG_INFO("b: {:016b} t: {:03}", md.button_data.button_data, md.button_data.trigger_data);

    constexpr Color black = {0, 0, 0};
    constexpr Color white = {255, 255, 255};
    constexpr Color light_gray = {200, 200, 200};
    constexpr Color red = {255, 0, 0};
    constexpr Color green = {0, 255, 0};
    constexpr Color blue = {0, 0, 255};
    constexpr Color pink = {255, 105, 180};
    constexpr Color cyan = {64, 128, 255};

    auto draw_centered_box = [&, this](s32 x, s32 y, s32 w, s32 h, Color c, bool pressed) {
        s32 amp = pdata.analogButtons.l2 >> 6;
        s32 vx = (rand() % (2 * amp + 1)) - amp;
        s32 vy = (rand() % (2 * amp + 1)) - amp;
        scene->DrawRectangleWithBorder(1280 + 320 + x - (w / 2) + vx, 400 + y - (h / 2) + vy, w, h,
                                       pressed ? light_gray : c, 3, white);
    };
    auto& b = md.button_data.button_data;
    using OMB = OrbisMoveButtonDataOffset;
    s32 fill = int((float)md.button_data.trigger_data / 255.f * 75.f);
    auto& bc = move_ball_colour;
#define PRESSED(btn) (b & OrbisMoveButtonDataOffset::btn) != 0
    // clang-format off
    draw_centered_box(  0,  100, 150,  500, black,  false);             // body
    draw_centered_box(  0, -250, 170,  170,    bc,  false);             // ball
    draw_centered_box(-90,  -80,  20,   60, black,  PRESSED(Select));   // share
    draw_centered_box( 90,  -80,  20,   60, black,  PRESSED(Start));    // options
    draw_centered_box(  0,  -50,  34,   80, black,  PRESSED(Move));     // move
    draw_centered_box(  0,   30,  34,   34, black,  false);             // ps
    draw_centered_box(  0,  100,  20,   75, black,  false);             // t frame
    draw_centered_box(  0,  100,  20, fill, white,  false);             // t fill
    draw_centered_box(-40,  -30,  20,   20,  blue,  PRESSED(Cross));    // x
    draw_centered_box(-40,  -70,  20,   20,  pink,  PRESSED(Square));   // []
    draw_centered_box( 40,  -30,  20,   20,   red,  PRESSED(Circle));   // o
    draw_centered_box( 40,  -70,  20,   20, green,  PRESSED(Triangle)); // ^
    draw_centered_box(  0,  300,  20,   10,   red,  false);             // power
#undef PRESSED
    auto& g = md.gyro;
    auto& a = md.accelerometer;
    scene->DrawLine(1280 + 200, 800 + 140, s32(g[0] * -7), s32(g[0] * 7), 3, {0, 255, 255});
    scene->DrawLine(1280 + 200, 800 + 140, s32(g[1] * -10), 0, 3, {0, 255, 255});
    scene->DrawLine(1280 + 200, 800 + 140, 0, s32(g[2] * 10), 3, {0, 255, 255});

    scene->DrawLine(1280 + 440, 800 + 140, s32(a[1] * 20), s32(a[1] * -20), 3, {0, 255, 255});
    scene->DrawLine(1280 + 440, 800 + 140, s32(a[0] * -30), 0, 3, {0, 255, 255});
    scene->DrawLine(1280 + 440, 800 + 140, 0, s32(a[2] * 30), 3, {0, 255, 255});
    // clang-format on
}

void App::DrawMoveTrackerResult() {
    if (!use_tracking) {
        return;
    }
    if (state.mt_status == Status::Tracking) {
        LOG_INFO("x: {}, y: {}, z: {}", mt_state.position.x, mt_state.position.y,
                 mt_state.position.z);
    }
#ifdef GRAPHICS_USES_FONT
    std::string pos_text =
        fmt::format("x: {:+07.3f}\ny: {:+07.3f}\nz: {:+07.3f}", mt_state.position.x,
                    mt_state.position.y, mt_state.position.z);
    scene->DrawText(pos_text.c_str(), font, 50, 850, {0, 0, 0}, {255, 255, 255});

    std::string cam_stats_text = fmt::format("cam pitch: {:+07.3f}, roll: {:+07.3f}",
                                             mt_state.cameraPitchAngle, mt_state.cameraRollAngle);
    scene->DrawText(cam_stats_text.c_str(), font, 50, 1000, {0, 0, 0}, {255, 255, 255});

    auto const& acc = mt_state.acceleration;
    std::string accel_text =
        fmt::format("dx: {:+07.3f}\ndy: {:+07.3f}\ndz: {:+07.3f}", acc.x, acc.y, acc.z);
    scene->DrawText(accel_text.c_str(), font, 400, 850, {0, 0, 0}, {255, 255, 255});
#endif
}

void App::DrawLoadingOverlay() {
    scene->DrawRectangle(400 + 000, 375, 50, 50, {255, 255, 255});
    scene->DrawRectangle(400 + 200, 375, 50, 50, {255, 255, 255});
    scene->DrawRectangle(400 + 400, 375, 50, 50, {255, 255, 255});
}
