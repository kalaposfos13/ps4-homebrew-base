#include "app.h"

int main(void) {
    App app{};
    app.InitCamera();
    app.InitTrackers();

    while (app.HandleInput()) {
        if (!app.UpdateCamera()) {
            continue;
        }
        app.UpdatePadTracker();
        app.UpdateMoveTracker();
        app.DrawFrame();
    }
}
