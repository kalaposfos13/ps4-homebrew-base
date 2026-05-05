#include "assert.h"
#include "renderer.h"

Renderer::Renderer() {
    Init();
}

Renderer::~Renderer() {
    FT_Done_Face(font);
}

void Renderer::Init() {
    if (!scene) {
        scene = new Scene2D(1920, 1080, 4);
        ASSERT_MSG(scene->Init(0xd000000, 2), "Failed to initialize 2D scene");
    }
    if (use_font && scene->ftLib) {
        ASSERT_OK(scene->InitFontLib());
        std::string font_path =
            fmt::format("/{}/common/font/DFHEI5-SONY.ttf", sceKernelGetFsSandboxRandomWord());
        ASSERT_MSG(scene->InitFont(&font, font_path.c_str(), 80) && font != nullptr,
                   "Failed to init font");
    }
}

void Renderer::BeginFrame() {
    scene->FrameBufferClear();
}

void Renderer::EndFrame() {
    scene->SubmitFlip();
    scene->FrameWait();
    scene->FrameBufferSwap();
}

void Renderer::DrawImage(const Image& img, int x, int y) {
    auto* fb = (uint32_t*)scene->frameBuffers[scene->activeFrameBufferIdx].left;
    ASSERT(fb != nullptr);
    if (img.pixels == nullptr)
        return;

    for (int j = 0; j < img.height; j++) {
        for (int i = 0; i < img.width; i++) {
            int sx = x + i;
            int sy = y + j;

            if (sx < 0 || sy < 0 || sx >= scene->width || sy >= scene->height)
                continue;

            fb[sy * scene->width + sx] = img.pixels[j * img.width + i];
        }
    }
}

void Renderer::DrawImage(const Image& img, int dstX, int dstY, int dstW, int dstH, int cropL,
                         int cropR, int cropT, int cropB, float zoom) {
    auto* fb = (uint32_t*)scene->frameBuffers[scene->activeFrameBufferIdx].left;
    ASSERT(fb != nullptr);
    if (!img.pixels)
        return;

    int srcW = img.width - cropL - cropR;
    int srcH = img.height - cropT - cropB;

    if (srcW <= 0 || srcH <= 0)
        return;

    // --- compute scale with zoom ---
    float baseScale = std::min((float)dstW / srcW, (float)dstH / srcH); // fit (letterbox)
    float scale = baseScale * zoom;
    // // clamp so we never exceed fill unless explicitly zooming in
    // float maxScale = std::max((float)dstW / srcW, (float)dstH / srcH);
    // if (scale > maxScale)
    //     scale = maxScale;

    // scaled size
    int drawW = (int)(srcW * scale);
    int drawH = (int)(srcH * scale);

    // center inside destination rect
    int offsetX = dstX + (dstW - drawW) / 2;
    int offsetY = dstY + (dstH - drawH) / 2;

    // compute destination rect bounds
    int rectStartX = dstX;
    int rectEndX = dstX + dstW;
    int rectStartY = dstY;
    int rectEndY = dstY + dstH;

    // clamp against both framebuffer AND destination rect
    int startX = std::max({0, offsetX, rectStartX});
    int endX = std::min({scene->width, offsetX + drawW, rectEndX});

    int startY = std::max({0, offsetY, rectStartY});
    int endY = std::min({scene->height, offsetY + drawH, rectEndY});

    // precompute inverse ratios
    float invW = (float)srcW / drawW;
    float invH = (float)srcH / drawH;

    for (int dy = startY; dy < endY; dy++) {
        int y = dy - offsetY;
        int sy = cropT + (int)(y * invH);

        uint32_t* dstRow = fb + dy * scene->width;

        for (int dx = startX; dx < endX; dx++) {
            int x = dx - offsetX;

            // keep your horizontal flip
            int sx = cropL + (int)((drawW - 1 - x) * invW);

            dstRow[dx] = img.pixels[sy * img.stride + sx];
        }
    }
}