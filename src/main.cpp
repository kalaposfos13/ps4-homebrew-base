#include "app.h"

int main(void) {
    App app{};
    app.use_dumped_frame = false;
    app.use_tracking = false;

    app.InitCamera();
    app.InitMove();
    app.InitMoveTracker();

    while (app.HandleControllerInput()) {
        if (!app.UpdateCamera()) {
            // continue;
        }
        app.UpdateMove();
        app.UpdateMoveTracker();
        app.FrameStart();
        app.DrawCameraImage();
        app.DrawMoveResult();
        app.DrawMoveTrackerResult();
        app.FrameEnd();
    }
}
