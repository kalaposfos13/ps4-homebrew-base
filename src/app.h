#pragma once

#include "assert.h"
#include "camera.h"
#include "gnm.h"
#include "graphics.h"
#include "logging.h"
#include "movetracker.h"
#include "padtracker.h"
#include "renderer.h"
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
#include <vector>

enum Status : u32 {
    Tracking,
    NotTracking,
    Calibrating,
    Error,
};

struct AppState {
    s32 eye = 0;
    Status pt_status[ORBIS_CAMERA_MAX_DEVICE_NUM] = {Status::Calibrating, Status::Calibrating};
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
    void DrawDebugStuff();
    void DrawLoadingFrame();

    bool use_dumped_frame = true;
    void* dumped_frame_buf[2]{};

    s32 user_id{};
    s32 pad_handle{};
    s32 move_handle{};

    OrbisPadData pdata{};
    OrbisPadTrackerInput pt_input{};
    OrbisPadTrackerData pt_output{};
    OrbisMoveTrackerImage mt_images[ORBIS_MOVE_MAX_IMAGES]{};
    OrbisMoveData m_data[ORBIS_MOVE_MAX_CONTROLLERS]{};
    OrbisMoveTrackerControllerInput mt_controllers[ORBIS_MOVE_MAX_CONTROLLERS]{};
    OrbisMoveTrackerState mt_state{};

    Renderer renderer{};
    AppState state{};
    FT_Face font{};
    OrbisCameraExposureGain exposuregain{
        .exposureControl = 0,
        .exposure = 83,
        .gain = 100,
        .mode = 0,
    };

    OrbisPadTrackerInput pad_input{};
    OrbisPadTrackerData pad_output{};

    OrbisMoveTrackerImage move_images[ORBIS_CAMERA_MAX_DEVICE_NUM]{};
    OrbisMoveTrackerControllerInput move_controllers[ORBIS_MOVE_MAX_CONTROLLERS]{};
    OrbisMoveTrackerState move_state{};
};
