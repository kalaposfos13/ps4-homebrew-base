#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
// #include <orbis/Camera.h>
#include <camera.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include "assert.h"
#include "logging.h"
#include "types.h"

s32 user_id, camera_handle;

void init_libs() {
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    sceUserServiceGetInitialUser(&user_id);
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

    ASSERT((camera_handle = sceCameraOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, 0, 0, nullptr)) >= 0);
    LOG_INFO("PlayStation Camera connected (handle: {})", camera_handle);

    OrbisCameraConfig cconfig{};
    cconfig.size_this = sizeof(OrbisCameraConfig);
    cconfig.config_type = ORBIS_CAMERA_CONFIG_TYPE1;
    ASSERT(sceCameraSetConfig(camera_handle, &cconfig) == ORBIS_OK);

    OrbisCameraStartParameter cstart_param{};
    cstart_param.size_this = sizeof(OrbisCameraStartParameter);
    cstart_param.format_level[0] = ORBIS_CAMERA_FRAME_FORMAT_LEVEL0;
    ASSERT(sceCameraStart(camera_handle, &cstart_param) == ORBIS_OK);

    OrbisCameraVideoSyncParameter cvsync_param{};
    cvsync_param.size_this = sizeof(OrbisCameraVideoSyncParameter);
    cvsync_param.video_sync_mode = 1;
    ASSERT(sceCameraSetVideoSync(camera_handle, &cvsync_param) == ORBIS_OK);
    LOG_INFO("PlayStation Camera set up and started.");

    for (int i = 0; i < 100; i++) {
        OrbisCameraFrameData frameData;
        frameData.size_this = (sizeof(OrbisCameraFrameData));
        frameData.read_mode = 0;
        if (sceCameraGetFrameData(camera_handle, &frameData) != ORBIS_OK) {
            LOG_INFO("Couldn't obtain frame data.");
            continue;
        }
        if (sceCameraIsValidFrameData(camera_handle, &frameData) == 0) {
            LOG_INFO("Invalid frame, most likely a me skill issue.");
            continue;
        }
        // LOG_INFO("Got valid frame, ptr: {}", frameData.frame_ptr_list[0][0]);
        // dump_frame_data(frameData);
    }

    sceCameraStop(camera_handle);
    sceCameraClose(camera_handle);
    LOG_INFO("PlayStation Camera closed.");
    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
