#include "assert.h"
#include "camera.h"
#include "gnm.h"
#include "graphics.h"
#include "logging.h"
#include "movetracker.h"
#include "padtracker.h"
#include "types.h"

#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
// #include <orbis/Camera.h>
// #include <orbis/GnmDriver.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <sys/mman.h>

#define STUB_WEAK(name)                                                                            \
    extern "C" void name() {                                                                       \
        printf("called " #name);                                                                   \
        asm volatile("ud2");                                                                       \
    }
STUB_WEAK(__assert)
FILE* __stderrp = stdout;

s32 user_id, camera_handle, pad_handle, move_handle, frame_id = 0;
Scene2D* scene;

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

static inline uint8_t Convert12to8(uint16_t v) {
    return v >> 4;
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

void init_libs() {
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_MOVE);
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

    sceSysmoduleLoadModule(ORBIS_SYSMODULE_PAD_TRACKER);
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_MOVE_TRACKER);
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

void pad_get_print_info() {
    LOG_INFO("scePadGetInfo");
    constexpr s32 size = 8;
    u32 pad_info[size]{};
    ASSERT_OK(scePadGetInfo(pad_info));
    for (int i = 0; i < size; i++) {
        LOG_INFO("{:08x}\n", pad_info[i]);
    }
    return;
}

enum Status : u32 {
    Tracking,
    NotTracking,
    Calibrating,
    Error,
};

Status update_and_get_pad_tracker_data(OrbisPadTrackerInput& pt_input,
                                       OrbisCameraFrameData const& frame_data,
                                       OrbisPadTrackerData& pt_output) {
    pt_input.images[0].data = frame_data.frame_ptr_list[0][0];
    pt_input.images[1].data = frame_data.frame_ptr_list[1][0];

    ASSERT_OK(scePadTrackerUpdate(pt_input));
    scePadTrackerReadState(pad_handle, &pt_output);
    switch (pt_output.imageCoordinates[0].status) {
    case ORBIS_PAD_TRACKER_TRACKING:
        return Status::Tracking;
    case ORBIS_PAD_TRACKER_NOT_TRACKING:
        return Status::NotTracking;
    case ORBIS_PAD_TRACKER_CALIBRATING:
        return Status::Calibrating;
    default:
        UNREACHABLE();
        return Status::Error;
    }
}

Status update_and_get_move_tracker_data(
    OrbisCameraFrameData const& frame_data,
    OrbisMoveTrackerControllerInput mt_controllers[ORBIS_MOVE_MAX_CONTROLLERS],
    OrbisMoveTrackerImage mt_images[ORBIS_CAMERA_MAX_DEVICE_NUM], OrbisMoveTrackerState& mt_state) {
    OrbisMoveData m_data[ORBIS_MOVE_MAX_CONTROLLERS];
    auto now = sceKernelGetProcessTime();
    mt_images[0].timestamp = now;
    mt_images[1].timestamp = now;
    mt_images[0].data = frame_data.frame_ptr_list[0][0];
    mt_images[1].data = frame_data.frame_ptr_list[1][0];
    ASSERT_OK(sceMoveTrackerCameraUpdate(mt_images, frame_data.meta.acceleration));
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
    ASSERT_NO_ERROR(sceMoveReadStateLatest(move_handle, mt_controllers[0].data));
    ASSERT_OK(sceMoveTrackerControllersUpdate(mt_controllers));
    s32 get_state_status = 0;
    ASSERT_NO_ERROR(get_state_status = sceMoveTrackerGetState(move_handle, s64(-1), &mt_state));
    if (get_state_status != 1)
        LOG_INFO("sceMoveTrackerGetState: {}", get_state_status);
    if ((mt_state.flags & 1) != 0) {
        return Status::Tracking;
    } else {
        return Status::NotTracking;
    }
}

int main(void) {
    init_libs();

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

    OrbisCameraExposureGain eg{};
    sceCameraGetExposureGain(camera_handle, 1, &eg, nullptr);

    LOG_INFO("exposure: {}, gain: {}", eg.exposure, eg.gain);

    OrbisCameraFrameData frame_data{};
    frame_data.size_this = (sizeof(OrbisCameraFrameData));
    frame_data.read_mode = 0;
    frame_data.meta.exposureGain[0].exposureControl = 0;
    frame_data.meta.exposureGain[1].exposureControl = 0;

    OrbisPadTrackerInput pt_input{};
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

    OrbisPadTrackerData pt_output{};

    OrbisMoveTrackerImage mt_images[ORBIS_CAMERA_MAX_DEVICE_NUM]{};
    mt_images[0].exposure = eg.exposure;
    mt_images[0].gain = eg.gain;
    mt_images[1].exposure = eg.exposure;
    mt_images[1].gain = eg.gain;

    OrbisMoveData m_data[ORBIS_MOVE_MAX_CONTROLLERS]{};
    OrbisMoveTrackerControllerInput mt_controllers[ORBIS_MOVE_MAX_CONTROLLERS]{};
    OrbisMoveTrackerState mt_state{};

    int eye = 0, sq_pressed = false;

    // camera warmup and tracker calibration
    ASSERT_OK(scePadTrackerCalibrate());
    ASSERT_OK(sceMoveTrackerCalibrateReset());
    bool pad_tracker_calibrated = false, move_tracker_calibrated = false;
    for (int i = 0; i < 100; i++) {
        if (sceCameraGetFrameData(camera_handle, &frame_data) != ORBIS_OK) {
            sceKernelUsleep(10000);
            continue;
        }
        if (!sceCameraIsValidFrameData(camera_handle, &frame_data)) {
            sceKernelUsleep(10000);
            continue;
        }

        if (update_and_get_pad_tracker_data(pt_input, frame_data, pt_output) == Status::Tracking) {
            pad_tracker_calibrated = true;
        }
        if (update_and_get_move_tracker_data(frame_data, mt_controllers, mt_images, mt_state) ==
            Status::Tracking) {
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

    LOG_INFO("finished camera warmup, tracker is tracking");

    while (true) {
        // pad_get_print_info();
        OrbisPadData pdata;
        scePadReadState(pad_handle, &pdata);
        if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_CIRCLE) != 0) {
            break;
        }
        if ((pdata.buttons & OrbisPadButton::ORBIS_PAD_BUTTON_SQUARE) != 0) {
            if (!sq_pressed) {
                eye = 1 - eye;
                sq_pressed = true;
            }
        } else {
            sq_pressed = false;
        }
        if (sceCameraGetFrameData(camera_handle, &frame_data) != ORBIS_OK) {
            // LOG_INFO("Couldn't obtain frame data.");
            sceKernelUsleep(10000);
            continue;
        }
        if (sceCameraIsValidFrameData(camera_handle, &frame_data) == 0) {
            LOG_INFO("Invalid frame, most likely a me skill issue.");
            continue;
        }
        // LOG_INFO("Got valid frame, ptr: {} {}", frameData.frame_ptr_list[0][0],
        // frameData.frame_ptr_list[1][0]); dump_frame_data(frame_data);

        scene->FrameBufferClear();

        // for the real thing
        if (eye == 0) {
            DrawRAW16Frame(scene, frame_data.frame_ptr_list[eye][0], 1280, 800);
        } else {
            DrawRAW16Frame(scene, frame_data.frame_ptr_list[eye][0], 1280, 800);
        }

        // for my webcam that I use for testing shadPS4
        // DrawYUV422Frame(scene, frameData.frame_ptr_list[eye][0], 640, 480);
        // for the capture card
        // DrawYUV422Frame(scene, frameData.frame_ptr_list[eye][0], 1920, 1080);

        // draw tracker output
        auto status = update_and_get_pad_tracker_data(pt_input, frame_data, pt_output);
        if (status == Status::Tracking) {
            scene->DrawRectangle((pt_output.imageCoordinates[0].x * 1280) - 0,
                                 (pt_output.imageCoordinates[0].y * 800) - 0, 10, 10, {255, 0, 0});
        }

        status = update_and_get_move_tracker_data(frame_data, mt_controllers, mt_images, mt_state);
        if ((mt_state.flags & 1) == 1) {
            LOG_INFO("Move controller is tracking, x: {}, y: {}, z: {}", mt_state.position.x,
                     mt_state.position.y, mt_state.position.z);
        }

        // Submit the frame buffer
        scene->SubmitFlip(frame_id);
        scene->FrameWait(frame_id);

        // Swap to the next buffer
        scene->FrameBufferSwap();
        frame_id++;
    }

    sceCameraStop(camera_handle);
    sceCameraClose(camera_handle);
    LOG_INFO("PlayStation Camera closed.");
    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
