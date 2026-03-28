#include "app.h"
#include "algos.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>

const char* dump_path[2] = {"/data/homebrew/frame_dump1.bin", "/data/homebrew/frame_dump2.bin"};

using OMB = OrbisMoveButtonDataOffset;

MoveScreenState ConvertMoveDataToScreenDimensions(const OrbisMoveTrackerState* s) {
    const float width = 1280.0f;
    const float height = 800.0f;

    const float cx = width * 0.5f;
    const float cy = height * 0.5f;

    const float fx = 1100.0f;
    const float fy = 1000.0f;

    const float ball_radius = 0.022f;
    const float handle_length = 0.15f;

    float X = s->position.x;
    float Y = s->position.y;
    float Z = s->position.z;

    // Convert WORLD → CAMERA coordinates
    rotate_x(&Y, &Z, -s->cameraPitchAngle);
    rotate_z(&X, &Y, -s->cameraRollAngle);

    MoveScreenState out = {};

    if (Z <= 0.001f || !std::isfinite(Z))
        return out;

    // Project center
    float xn = X / Z;
    float yn = Y / Z;

    // radial distortion
    float r2 = xn * xn + yn * yn;

    float k1 = -0.25;
    float k2 = 0.15;

    float factor = 1.0f + k1 * r2 + k2 * r2 * r2;

    xn *= factor;
    yn *= factor;

    // then project
    out.x = fx * xn + cx;
    out.y = cy - fy * yn;

    // Project sphere radius
    out.radius = fx * (ball_radius / Z);

    // Handle direction vector (0,0,1)
    float dir[3] = {0, 0, 1};
    quat_rotate(s->orientation, dir);
    rotate_x(&dir[1], &dir[2], -s->cameraPitchAngle);
    rotate_z(&dir[0], &dir[1], -s->cameraRollAngle);

    // Handle end in world space
    float endX = X + dir[0] * handle_length;
    float endY = Y + dir[1] * handle_length;
    float endZ = Z + dir[2] * handle_length;

    // Project handle end
    float end2D_x = fx * (endX / endZ) + cx;
    float end2D_y = cy - fy * (endY / endZ);

    float dx = end2D_x - out.x;
    float dy = end2D_y - out.y;

    // Apparent length
    out.length = sqrtf(dx * dx + dy * dy);

    // 2D rotation angle
    out.angle = atan2f(dy, dx);

    return out;
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
    // sceSysmoduleLoadModule(ORBIS_SYSMODULE);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_MOVE);

    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    scePadInit();
    ASSERT_NO_ERROR(pad_handle = scePadOpen(user_id, 0, 0, 0));
    LOG_INFO("userid: {}, pad handle: {:x}", user_id, pad_handle);
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
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_MOVE_TRACKER);

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
    ASSERT_OK(sceMoveTrackerInit(mt_onion, mt_garlic, 0, 0));
    sceMoveResetLightSphere(move_handle);
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
    ASSERT_MSG(scene->InitFont(&font, font_path.c_str(), 40) && font != nullptr,
               "Failed to init font");
}

void App::UpdateMoveTracker() {
    if (!use_tracking) {
        return;
    }
    auto now = sceKernelGetProcessTime();
    mt_images[0].timestamp = now;
    // mt_images[1].timestamp = now;
    mt_images[0].data = frame_data.frame_ptr_list[0][0];
    // mt_images[1].data = frame_data.frame_ptr_list[1][0];
    ASSERT_OK(sceMoveTrackerControllersUpdate(mt_controllers));
    ASSERT_OK(sceMoveTrackerCameraUpdate(mt_images, frame_data.meta.acceleration));
    s32 get_state_status = 0;
    ASSERT_NO_ERROR(get_state_status = sceMoveTrackerGetState(
                        move_handle, now + ORBIS_MOVE_TRACKER_LATENCY, &mt_state));
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

    return true;
}

u8 App::GetVibrationStrength() {
    return std::max(
        pdata.analogButtons.l2,
        (u8)(m_data->button_data.trigger_data *
             ((m_data->button_data.button_data & OMB::Move) != 0 ? 1 : 0)));
}

bool App::HandleMoveInput() {
    ASSERT_NO_ERROR(sceMoveReadStateLatest(move_handle, m_data));
    static std::map<OMB, bool> m_btn_pressed{};
    auto is_m_button_pressed = [&, this](OMB b) {
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
    auto is_m_button_down = [&, this](OMB b) {
        return ((m_data[0].button_data.button_data & b) != 0);
    };
    auto is_t_plus_m_button_pressed = [&](OMB b) {
        return is_m_button_pressed(b) &&
        is_m_button_down(OMB::T);
    };
    if (is_t_plus_m_button_pressed(OMB::Cross)) {
        s32 r = rand() % 8;
        move_ball_colour.r = (r & 0b100) ? 255 : 0;
        move_ball_colour.g = (r & 0b010) ? 255 : 0;
        move_ball_colour.b = (r & 0b001) ? 255 : 0;
        sceMoveSetLightSphere(move_handle, move_ball_colour.r, move_ball_colour.g,
                              move_ball_colour.b);
    }
    if (is_t_plus_m_button_pressed(OMB::Circle)) {
        LOG_INFO("Exiting");
        return false;
    }
    if (is_t_plus_m_button_pressed(OMB::Square)) {
        if (!use_tracking) {
            LOG_INFO("Enabling tracking");
            use_tracking = true;
            InitMoveTracker();
        }
    }
    if (is_t_plus_m_button_pressed(OMB::Triangle)) {
        if (!use_font) {
            LOG_INFO("Enabling fonts");
            use_font = true;
            InitFont();
        }
    }
    sceMoveSetVibration(move_handle, GetVibrationStrength());

    return true;
}

void App::FrameStart() {
    scene->DrawRectangle(1280, 0, 1920 - 1280, 1080, {75, 75, 75});
    scene->DrawRectangle(0, 800, 1280, 1080 - 800, {75, 75, 75});
    DrawPlaceholderCameraImage();
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
    LOG_CALL(sceMoveSetLightSphere(move_handle, move_ball_colour.r, move_ball_colour.g,
                                   move_ball_colour.b));
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
        s32 amp = GetVibrationStrength() >> 6;
        s32 vx = (rand() % (2 * amp + 1)) - amp;
        s32 vy = (rand() % (2 * amp + 1)) - amp;
        scene->DrawRectangleWithBorder(1280 + 320 + x - (w / 2) + vx, 400 + y - (h / 2) + vy, w, h,
                                       pressed ? light_gray : c, 3, white);
    };
    auto& b = md.button_data.button_data;
    s32 fill = int((float)md.button_data.trigger_data / 255.f * 75.f);
    auto& bc = move_ball_colour;
#define PRESSED(btn) (b & OMB::btn) != 0
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
    scene->DrawLine(1280 + 200, 800 + 140, std::clamp( s32(g[0] * -7), -100, 100), std::clamp(  s32(g[0] * 7), -100, 100), 3, {0, 255, 255});
    scene->DrawLine(1280 + 200, 800 + 140, std::clamp(s32(g[1] * -10), -100, 100), std::clamp(              0, -100, 100), 3, {0, 255, 255});
    scene->DrawLine(1280 + 200, 800 + 140, std::clamp(              0, -100, 100), std::clamp( s32(g[2] * 10), -100, 100), 3, {0, 255, 255});
    scene->DrawLine(1280 + 440, 800 + 140, std::clamp( s32(a[1] * 20), -100, 100), std::clamp(s32(a[1] * -20), -100, 100), 3, {0, 255, 255});
    scene->DrawLine(1280 + 440, 800 + 140, std::clamp(s32(a[0] * -30), -100, 100), std::clamp(              0, -100, 100), 3, {0, 255, 255});
    scene->DrawLine(1280 + 440, 800 + 140, std::clamp(              0, -100, 100), std::clamp( s32(a[2] * 30), -100, 100), 3, {0, 255, 255});
    // clang-format on
}

void App::DrawMoveTrackerResult() {
    if (!use_tracking) {
        return;
    }
    if (state.mt_status == Status::Calibrating) {
        DrawLoadingOverlay();
        return;
    }
    auto const& msd = ConvertMoveDataToScreenDimensions(&mt_state);
    scene->DrawRectangle(msd.x - (msd.radius), msd.y - (msd.radius), msd.radius * 2, msd.radius * 2,
                         {0, 0, 255});
    scene->DrawLine(msd.x, msd.y, cosf(msd.angle) * std::min(msd.length, 200.f),
                    sinf(msd.angle) * std::min(msd.length, 200.f), 8, {0, 0, 0});
    if (!use_font) {
        return;
    }
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
}

void App::DrawLoadingOverlay() {
    scene->DrawRectangle(400 + 000, 375, 50, 50, {255, 255, 255});
    scene->DrawRectangle(400 + 200, 375, 50, 50, {255, 255, 255});
    scene->DrawRectangle(400 + 400, 375, 50, 50, {255, 255, 255});
}
