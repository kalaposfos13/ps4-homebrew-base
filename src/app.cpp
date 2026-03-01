#include "app.h"

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
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_PAD_TRACKER);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_MOVE_TRACKER);

    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    scePadInit();
    ASSERT_NO_ERROR(pad_handle = scePadOpen(user_id, 0, 0, 0));
    LOG_INFO("userid: {}, pad handle: {:x}", user_id, pad_handle);

    ASSERT_OK(sceMoveInit());
    move_handle = sceMoveOpen(user_id, /*standard*/ 0, 0);

    int frameID = 0;
    scene = new Scene2D(1920, 1080, 4);
    ASSERT_MSG(scene->Init(0xC000000, 2), "Failed to initialize 2D scene");
    DrawLoadingFrame();
}

App::~App() {
    sceCameraStop(camera_handle);
    sceCameraClose(camera_handle);
    LOG_INFO("App stopped.");
    sceSystemServiceLoadExec("EXIT", nullptr);
}

void App::InitCamera() {
    if (sceCameraIsAttached(0) != 1) {
        LOG_NOTIFICATION("Please connect the PlayStation Camera.");
    }
    while (sceCameraIsAttached(0) != 1) {
        LOG_INFO("Please connect the PlayStation Camera.");
        sceKernelSleep(1);
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
}

void App::InitTrackers() {
    LOG_INFO("Setting up trackers");
    OrbisCameraExposureGain eg{};
    sceCameraGetExposureGain(camera_handle, 1, &eg, nullptr);

    frame_data.size_this = (sizeof(OrbisCameraFrameData));
    frame_data.read_mode = 0;
    frame_data.meta.exposureGain[0].exposureControl = 0;
    frame_data.meta.exposureGain[1].exposureControl = 0;

    pt_input.handles[0] = pad_handle;
    pt_input.handles[1] = -1;
    pt_input.handles[2] = -1;
    pt_input.handles[3] = -1;

    pt_input.images[0].exposure = eg.exposure;
    pt_input.images[0].gain = eg.gain;
    pt_input.images[0].width = 1280;
    pt_input.images[0].height = 800;
    pt_input.images[1].exposure = eg.exposure;
    pt_input.images[1].gain = eg.gain;
    pt_input.images[1].width = 1280;
    pt_input.images[1].height = 800;

    pt_input.images[0].data = nullptr;
    pt_input.images[1].data = nullptr;

    mt_images[0].exposure = eg.exposure;
    mt_images[0].gain = eg.gain;
    mt_images[1].exposure = eg.exposure;
    mt_images[1].gain = eg.gain;

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

    s32 pt_o_s, pt_g_s, mt_o_s, mt_g_s;
    ASSERT_OK(scePadTrackerGetWorkingMemorySize(&pt_o_s, &pt_g_s));
    ASSERT_OK(sceMoveTrackerGetWorkingMemorySize(&mt_o_s, &mt_g_s));
    ASSERT(pt_o_s > 0 && pt_g_s > 0);
    LOG_INFO("needed onion size: {:#x}, needed garlic size: {:#x}", pt_o_s, pt_g_s);
    LOG_INFO("needed onion size: {:#x}, needed garlic size: {:#x}", mt_o_s, mt_g_s);
    VAddr pt_onion = alloc_memory(pt_o_s, 0, ORBIS_KERNEL_WB_ONION,
                                  MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite);
    VAddr pt_garlic = alloc_memory(pt_g_s, 0, ORBIS_KERNEL_WC_GARLIC,
                                   MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite);
    VAddr mt_onion = alloc_memory(mt_o_s, 0, ORBIS_KERNEL_WB_ONION,
                                  MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite);
    VAddr mt_garlic = alloc_memory(mt_g_s, 0, ORBIS_KERNEL_WC_GARLIC,
                                   MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite);
    ASSERT_OK(scePadTrackerInit(pt_onion, pt_garlic, 0, 0));
    ASSERT_OK(sceMoveTrackerInit(mt_onion, mt_garlic, 0, 1));
}

void App::CalibrateTrackers() {
    // camera warmup and tracker calibration
    ASSERT_OK(scePadTrackerCalibrate());
    ASSERT_OK(sceMoveTrackerCalibrateReset());
    bool pad_tracker_calibrated = false, move_tracker_calibrated = false;
    for (int i = 0; i < 200; i++) {
        if (sceCameraGetFrameData(camera_handle, &frame_data) != ORBIS_OK) {
            sceKernelUsleep(10000);
            continue;
        }
        if (!sceCameraIsValidFrameData(camera_handle, &frame_data)) {
            sceKernelUsleep(10000);
            continue;
        }

        if (UpdatePadTracker() == Status::Tracking) {
            pad_tracker_calibrated = true;
        }
        if (UpdateMoveTracker() == Status::Tracking) {
            move_tracker_calibrated = true;
        }
        if (pad_tracker_calibrated && move_tracker_calibrated) {
            break;
        }
        sceKernelUsleep(10000);
    }
    if (!pad_tracker_calibrated) {
        LOG_NOTIFICATION("Pad tracking did not finish in time");
    }
    if (!move_tracker_calibrated) {
        LOG_NOTIFICATION("Move tracking did not finish in time");
    }
    LOG_INFO("finished camera warmup and tracker calibration");
}

bool App::UpdateCamera() {
    if (sceCameraGetFrameData(camera_handle, &frame_data) != ORBIS_OK) {
        sceKernelUsleep(10000);
        return false;
    }
    if (!sceCameraIsValidFrameData(camera_handle, &frame_data)) {
        return false;
    }
    return true;
}

Status App::UpdatePadTracker() {
    pt_input.images[0].data = frame_data.frame_ptr_list[0][0];
    pt_input.images[1].data = frame_data.frame_ptr_list[1][0];

    ASSERT_OK(scePadTrackerUpdate(pt_input));
    scePadTrackerReadState(pad_handle, &pt_output);
    switch (pt_output.imageCoordinates[0].status) {
    case ORBIS_PAD_TRACKER_TRACKING:
        state.pt_status = Status::Tracking;
        break;
    case ORBIS_PAD_TRACKER_NOT_TRACKING:
        state.pt_status = Status::NotTracking;
        break;
    case ORBIS_PAD_TRACKER_CALIBRATING:
        state.pt_status = Status::Calibrating;
        break;
    default:
        UNREACHABLE_MSG("pt_status: {}", (u32)pt_output.imageCoordinates[0].status);
        state.pt_status = Status::Error;
    }
    return state.pt_status;
}

Status App::UpdateMoveTracker() {
    OrbisMoveData m_data[ORBIS_MOVE_MAX_CONTROLLERS];
    auto now = sceKernelGetProcessTime();
    mt_images[0].timestamp = now;
    mt_images[1].timestamp = now;
    mt_images[0].data = frame_data.frame_ptr_list[0][0];
    mt_images[1].data = frame_data.frame_ptr_list[1][0];
    ASSERT_OK(sceMoveTrackerCameraUpdate(mt_images, frame_data.meta.acceleration));
    ASSERT_NO_ERROR(sceMoveReadStateLatest(move_handle, mt_controllers[0].data));
    ASSERT_OK(sceMoveTrackerControllersUpdate(mt_controllers));
    s32 get_state_status = 0;
    ASSERT_NO_ERROR(get_state_status = sceMoveTrackerGetState(move_handle, s64(-1), &mt_state));
    if (get_state_status != 1)
        LOG_INFO("sceMoveTrackerGetState: {}", get_state_status);
    if ((mt_state.flags & 1) != 0) {
        state.mt_status = Status::Tracking;
    } else {
        state.mt_status = Status::NotTracking;
    }
    return state.mt_status;
}

bool App::HandleInput() {
    scePadReadState(pad_handle, &pdata);
    if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE) != 0) {
        return false;
    }
    if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_SQUARE) != 0) {
        if (!state.sq_pressed) {
            state.current_eye = 1 - state.current_eye;
            state.sq_pressed = true;
        }
    } else {
        state.sq_pressed = false;
        }
    return true;
}

void App::DrawFrame() {
    scene->FrameBufferClear();

    switch (frame_data.meta.format[state.current_eye][0]) {
    case ORBIS_CAMERA_FORMAT_YUV422:
        DrawYUV422Frame(scene, frame_data.frame_ptr_list[state.current_eye][0], 1280, 800);
        break;
    case ORBIS_CAMERA_FORMAT_RAW16:
        DrawRAW16Frame(scene, frame_data.frame_ptr_list[state.current_eye][0], 1280, 800);
        break;
    case ORBIS_CAMERA_FORMAT_RAW8:
    default:
        UNREACHABLE();
        break;
    }

    if (state.pt_status == Status::Tracking) {
        scene->DrawRectangle((pt_output.imageCoordinates[0].x * 1280) - 0,
                             (pt_output.imageCoordinates[0].y * 800) - 0, 10, 10, {255, 0, 0});
    }

    if (state.mt_status == Status::Tracking) {
        LOG_INFO("Move controller is tracking, x: {}, y: {}, z: {}", mt_state.position.x,
                 mt_state.position.y, mt_state.position.z);
    }

    scene->SubmitFlip(frame_id);
    scene->FrameWait(frame_id);
    scene->FrameBufferSwap();
    frame_id++;
}

void App::DrawLoadingFrame() {
    scene->FrameBufferClear();

    scene->DrawRectangle(200 + 000, 500, 50, 50, {255, 255, 255});
    scene->DrawRectangle(200 + 200, 500, 50, 50, {255, 255, 255});
    scene->DrawRectangle(200 + 400, 500, 50, 50, {255, 255, 255});

    scene->SubmitFlip(frame_id);
    scene->FrameWait(frame_id);
    scene->FrameBufferSwap();
    frame_id++;
}
