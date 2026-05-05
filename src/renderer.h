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

    Scene2D* GetScene() { return scene; }

    bool use_font = false;
    Scene2D* scene{};
    FT_Face font{};
};