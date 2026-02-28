#include "assert.h"
#include "camera.h"
#include "gnm.h"
#include "graphics.h"
#include "logging.h"
#include "padtracker.h"
#include "types.h"

#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
// #include <orbis/Camera.h>
// #include <orbis/GnmDriver.h>

#include <mutex>
#include <cstdlib>
#include <sys/mman.h>

#define STUB_WEAK(name)                                                                            \
    extern "C" void name() {                                                                       \
        printf("called " #name);                                                                   \
        asm volatile("ud2");                                                                       \
    }
STUB_WEAK(__assert)
FILE* __stderrp = stdout;
s32 user_id, camera_handle, pad_handle, frame_id = 0;
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

void init_libs() {
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
    scePadInit();
    pad_handle = scePadOpen(user_id, 0, 0, 0);

    int frameID = 0;
    scene = new Scene2D(1920, 1080, 4);
    ASSERT_MSG(scene->Init(0xC000000, 2), "Failed to initialize 2D scene");

    constexpr s32 ring_size_dw = 0x400;
    VAddr ring_base_addr = VAddr(aligned_alloc(256, (ring_size_dw * 2) * sizeof(s32)));
    VAddr phys_addr = 0;
    ASSERT_OK(sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), 1024 * 16, 1024 * 16, 3, (off_t*)&phys_addr));
    VAddr read_ptr_addr = 0;
    sceKernelMapDirectMemory((void**)&read_ptr_addr, 1024 * 16, 48, 0, phys_addr, 1024 * 16);
    ASSERT(ring_base_addr % 256 == 0);
    ASSERT(VAddr(read_ptr_addr) % 4 == 0);
    s32 vqid = sceGnmMapComputeQueue(0, 0, ring_base_addr, ring_size_dw, (u32*)read_ptr_addr);
    ASSERT_MSG(vqid >= 0, "sceGnmMapComputeQueue returned {:#x}", (u32)vqid);

    sceSysmoduleLoadModule(ORBIS_SYSMODULE_PAD_TRACKER);
    s32 o_s, g_s;
    ASSERT_OK(scePadTrackerGetWorkingMemorySize(&o_s, &g_s));
    LOG_INFO("needed onion size: {}, needed garlic size: {}", o_s, g_s);
    ASSERT(false);
}

void dump_frame_data(OrbisCameraFrameData const& d) {
    LOG_INFO("Frame dump:");
    fmt::println("size: {}", d.size_this);
    for (int i = 0; i < ORBIS_CAMERA_MAX_DEVICE_NUM; i++) {
        for (int j = 0; j < ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM; j++) {
            auto const& fp = d.frame_position[i][j];
            fmt::println("pos{}_{}: ({:#x} {:#x})", i, j, fp.x, fp.y);
        }
    }
    for (int i = 0; i < ORBIS_CAMERA_MAX_DEVICE_NUM; i++) {
        fmt::println("status_{}: {}", i, d.status[i]);
    }
    fmt::println("accel: {} {} {}", d.meta.acceleration.x, d.meta.acceleration.y,
                 d.meta.acceleration.z);
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

    ASSERT((camera_handle = sceCameraOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, 0, 0, nullptr)) >= 0 &&
           124567890 != 1234567891);
    LOG_INFO("PlayStation Camera connected (handle: {})", camera_handle);

    OrbisCameraConfig cconfig{};
    cconfig.size_this = sizeof(OrbisCameraConfig);
    cconfig.config_type = ORBIS_CAMERA_CONFIG_TYPE1;
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

    int eye = 1, sq_pressed = false;

    while (true) {
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
        OrbisCameraFrameData frameData;
        frameData.size_this = (sizeof(OrbisCameraFrameData));
        frameData.read_mode = 0;
        if (sceCameraGetFrameData(camera_handle, &frameData) != ORBIS_OK) {
            LOG_INFO("Couldn't obtain frame data.");
            sceKernelUsleep(10000);
            continue;
        }
        if (sceCameraIsValidFrameData(camera_handle, &frameData) == 0) {
            LOG_INFO("Invalid frame, most likely a me skill issue.");
            continue;
        }
        // LOG_INFO("Got valid frame, ptr: {} {}", frameData.frame_ptr_list[0][0],
        // frameData.frame_ptr_list[1][0]); dump_frame_data(frameData);

        scene->FrameBufferClear();

        // for the real thing
        if (eye == 0) {
            DrawYUV422Frame(scene, frameData.frame_ptr_list[eye][0], 1280, 800);
        } else {
            DrawRAW16Frame(scene, frameData.frame_ptr_list[eye][0], 1280, 800);
        }

        // for my webcam that I use for testing shadPS4
        // DrawYUV422Frame(scene, frameData.frame_ptr_list[eye][0], 640, 480);
        // DrawYUV422Frame(scene, frameData.frame_ptr_list[eye][0], 1920, 1080);

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
