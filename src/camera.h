#pragma once

#include "image.h"
#include "orbis_camera.h"
#include "types.h"

class Camera {
public:
    Camera();
    ~Camera();
    bool Init();
    void Deinit();

    bool Update(); // fetch new frame (once per frame)

    static void ConvertYUV422(const void* yuv_buf, int w, int h, Image& out);
    static void ConvertRAW16(const void* raw16_buf, int w, int h, Image& out);
    bool RenderEyeToImage(int eye, int w, int h, Image& out);

    s32 handle{};
    OrbisCameraFrameData frame{};
    OrbisCameraExposureGain exposuregain{
        .exposureControl = 0,
        .exposure = 20,
        .gain = 100,
        .mode = 0,
    };
};
