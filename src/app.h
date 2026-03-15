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
#include <vector>

enum Status : u32 {
    Tracking,
    NotTracking,
    Calibrating,
    Error,
};

struct AppState {
    s32 eye = 0;
    Status mt_status = Status::Error;
};

class App {
public:
    App();
    ~App();

    bool InitCamera();
    bool UpdateCamera();
    void DrawCameraImage();
    void DrawPlaceholderCameraImage();

    void InitMove();
    bool HandleMoveInput();
    void DrawMoveResult();
    u8 GetVibrationStrength();

    void InitMoveTracker();
    void UpdateMoveTracker();
    void DrawMoveTrackerResult();

    void FrameStart();
    void FrameEnd();
    void DrawLoadingOverlay();

    bool HandleControllerInput();

    bool use_dumped_frame = false, use_tracking = false;
    void* dumped_frame_buf[2]{};

    s32 user_id{};
    s32 camera_handle{};
    s32 pad_handle{};
    s32 move_handle{};
    s32 frame_id{};

    OrbisPadData pdata{};

    OrbisMoveData m_data[ORBIS_MOVE_MAX_CONTROLLERS]{};
    Color move_ball_colour{0, 0, 255};

    OrbisCameraFrameData frame_data{};
    OrbisCameraExposureGain exposuregain{};

    OrbisMoveTrackerImage mt_images[ORBIS_MOVE_MAX_IMAGES]{};
    OrbisMoveTrackerControllerInput mt_controllers[ORBIS_MOVE_MAX_CONTROLLERS]{};
    OrbisMoveTrackerState mt_state{};

    Scene2D* scene{};
    AppState state{};
#ifdef GRAPHICS_USES_FONT
    FT_Face font{};
#endif
};
