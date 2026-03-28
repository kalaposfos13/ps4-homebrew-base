#include "app.h"

int main(void) {
    App app{};
    app.use_dumped_frame = false;
    app.use_font = false;

    app.InitGraphics();
    app.InitFont();
    app.InitCamera();
    app.InitMove();
    app.InitMoveTracker();

    while (app.HandleControllerInput() && app.HandleMoveInput()) {
        if (!app.UpdateCamera()) {
            // continue;
        }
        app.UpdateMoveTracker();
        app.FrameStart();
        app.DrawCameraImage();
        app.DrawMoveResult();
        app.DrawMoveTrackerResult();
        app.FrameEnd();
    }
}
