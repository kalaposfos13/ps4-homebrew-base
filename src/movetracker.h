#pragma once

#include "camera.h"
#include "types.h"

extern "C" {

// move

struct OrbisMoveDeviceInfo {
    float sphere_radius;
    float accelerometer_offset[3];
};

struct OrbisMoveButtonData {
    u16 button_data;
    u16 trigger_data;
};

struct OrbisMoveExtensionPortData {
    u16 status;
    u16 digital0;
    u16 digital1;
    u16 analog_right_x;
    u16 analog_right_y;
    u16 analog_left_x;
    u16 analog_left_y;
    unsigned char custom[5];
};

struct OrbisMoveData {
    float accelerometer[3];
    float gyro[3];
    OrbisMoveButtonData button_data;
    OrbisMoveExtensionPortData extension_data;
    s64 timestamp;
    s32 count;
    float temperature;
};

s32 sceMoveInit();
s32 sceMoveOpen(OrbisUserServiceUserId userId, s32 type, s32 index);
s32 sceMoveReadStateLatest(s32 handle, OrbisMoveData* pData);
s32 sceMoveSetLightSphere(s32 handle, u8 r, u8 g, u8 b);

// movetracker

constexpr s32 ORBIS_MOVE_MAX_CONTROLLERS = 4;
constexpr s32 ORBIS_MOVE_MAX_IMAGES = 2;
constexpr s32 ORBIS_MOVE_TRACKER_IMAGE_MAX = 2;
constexpr s32 ORBIS_MOVE_TRACKER_LATENCY = -22000;

struct OrbisMoveTrackerState {
    OrbisFVector3 position;
    OrbisFVector3 velocity;
    OrbisFVector3 acceleration;
    OrbisFQuaternion orientation;
    OrbisFVector3 angularVelocity;
    OrbisFVector3 angularAcceleration;
    OrbisFVector3 accelerometerPosition;
    OrbisFVector3 accelerometerVelocity;
    OrbisFVector3 accelerometerAcceleration;
    float cameraPitchAngle;
    float cameraRollAngle;
    OrbisMoveButtonData pad;
    OrbisMoveExtensionPortData ext;
    uint64_t timestamp;
    uint32_t flags;
};

struct OrbisMoveTrackerImage {
    void* data;
    int exposure;
    int gain;
    uint64_t timestamp;
};

struct OrbisMoveTrackerControllerInput {
    s32 handle;
    OrbisMoveData* data;
    s32 num;
};

s32 sceMoveTrackerCalibrateReset();
s32 sceMoveTrackerGetWorkingMemorySize(s32* onionSize, s32* garlicSize);
s32 sceMoveTrackerInit(VAddr onionMemory, VAddr garlicMemory, int pipeId, int queueId);
s32 sceMoveTrackerGetState(s32 handle, u64 timestamp, OrbisMoveTrackerState* state);
s32 sceMoveTrackerTerm();
s32 sceMoveTrackerCameraUpdate(OrbisMoveTrackerImage images[2], OrbisFVector3 cameraAccelerometer);
int32_t sceMoveTrackerControllersUpdate(
    OrbisMoveTrackerControllerInput controllerInputs[ORBIS_MOVE_MAX_CONTROLLERS]);
}