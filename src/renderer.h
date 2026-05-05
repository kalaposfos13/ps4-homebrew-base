#pragma once

#include "graphics.h"
#include "image.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Init();

    void BeginFrame();
    void EndFrame();

    void DrawImage(const Image& img, int x, int y);
    void DrawImage(const Image& img, int dstX, int dstY, int dstW, int dstH, int cropL, int cropR,
                   int cropT, int cropB, float zoom);

    Scene2D* GetScene() {
        return scene;
    }

    bool use_font = false;
    Scene2D* scene{};
    FT_Face font{};
};