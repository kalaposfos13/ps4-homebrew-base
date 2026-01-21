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
    LOG_INFO("PlayStation Camera set up and started.");

    sceCameraStop(camera_handle);
    sceCameraClose(camera_handle);
    LOG_INFO("PlayStation Camera closed.");
    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
