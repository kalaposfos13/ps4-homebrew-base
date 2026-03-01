#include "app.h"

int main(void) {
    App app{};
    app.InitCamera();
    // app.InitPadTracker();
    app.InitMoveTracker();

    while (app.HandleInput()) {
        if (!app.UpdateCamera()) {
            continue;
        }
        // app.UpdatePadTracker();
        app.UpdateMoveTracker();
        app.FrameStart();
        app.DrawCameraImage();
        // app.DrawPadTrackerResult();
        app.DrawMoveTrackerResult();
        app.FrameEnd();
    }
}
