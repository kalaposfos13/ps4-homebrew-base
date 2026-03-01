#pragma once

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

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

enum Status : u32 {
    Tracking,
    NotTracking,
    Calibrating,
    Error,
};

struct AppState {
    s32 eye = 0;
    bool sq_pressed = false;
    Status pt_status[ORBIS_CAMERA_MAX_DEVICE_NUM] = {Status::Error, Status::Error};
    Status mt_status = Status::Error;
};

class App {
public:
    App();
    ~App();
    void InitCamera();
    void InitPadTracker();
    void InitMoveTracker();
    bool UpdateCamera();
    void UpdatePadTracker();
    void UpdateMoveTracker();
    bool HandleInput();
    void FrameStart();
    void FrameEnd();
    void DrawCameraImage();
    void DrawPadTrackerResult();
    void DrawMoveTrackerResult();
    void DrawLoadingFrame();

    s32 user_id{};
    s32 camera_handle{};
    s32 pad_handle{};
    s32 move_handle{};
    s32 frame_id{};

    OrbisPadData pdata{};
    OrbisCameraFrameData frame_data{};
    OrbisPadTrackerInput pt_input{};
    OrbisPadTrackerData pt_output{};
    OrbisMoveTrackerImage mt_images[ORBIS_MOVE_MAX_IMAGES]{};
    OrbisMoveData m_data[ORBIS_MOVE_MAX_CONTROLLERS]{};
    OrbisMoveTrackerControllerInput mt_controllers[ORBIS_MOVE_MAX_CONTROLLERS]{};
    OrbisMoveTrackerState mt_state{};
    OrbisCameraExposureGain exposuregain{};

    Scene2D* scene{};
    AppState state{};

    OrbisCameraExposureGain exposure_gain{};

    OrbisPadTrackerInput pad_input{};
    OrbisPadTrackerData pad_output{};

    OrbisMoveTrackerImage move_images[ORBIS_CAMERA_MAX_DEVICE_NUM]{};
    OrbisMoveTrackerControllerInput move_controllers[ORBIS_MOVE_MAX_CONTROLLERS]{};
    OrbisMoveTrackerState move_state{};
};
