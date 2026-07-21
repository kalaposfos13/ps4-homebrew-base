#include <orbis/Sysmodule.h>
#include <orbis/VideoOut.h>
#include <orbis/libkernel.h>
#include <stdint.h>

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <proto-include.h>

// Color is used to pack together RGB information, and is used for every function that draws colored
// pixels.
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class Scene2D {

    int depth;
    int video;

    off_t directMemOff;
    size_t directMemAllocationSize;

    uintptr_t videoMemSP;
    void* videoMem;

    OrbisKernelEqueue flipQueue;
    OrbisVideoOutBufferAttribute attr;

    int frameBufferCount;

public:
    FT_Library ftLib;
    int width;
    int height;
    char** frameBuffers;
    int activeFrameBufferIdx;
    int frameBufferSize;
    int frame_id;

    bool initFlipQueue();
    bool allocateFrameBuffers(int num);
    char* allocateDisplayMem(size_t size);
    bool allocateVideoMem(size_t size, int alignment);
    void deallocateVideoMem();

    Scene2D(int w, int h, int pixelDepth);
    ~Scene2D();

    bool Init(size_t memSize, int numFrameBuffers);
    int InitFontLib();

    void SetActiveFrameBuffer(int index);
    void SubmitFlip();

    void FrameWait();
    void FrameBufferSwap();
    void FrameBufferClear();
    void FrameBufferFill(Color color);

    void DrawPixel(int const x, int const y, Color const color);
    void DrawPixel(int const x, int const y, int const color);
    void DrawRectangle(int const x, int const y, int const w, int const h, Color const color);
    void DrawRectangle(int const x, int const y, int const w, int const h, int const color);
    void DrawRectangleWithBorder(int const x, int const y, int const w, int const h,
                                 Color const color, int const b_w, Color const b_color);
    void DrawLine(int const p1x, int const p1y, int const dx, int const dy, int const w,
                  Color const c);

    bool InitFont(FT_Face* face, const char* fontPath, int fontSize);

    void DrawText(char const* txt, FT_Face face, int startX, int startY, Color bgColor,
                  Color fgColor);
};
#endif