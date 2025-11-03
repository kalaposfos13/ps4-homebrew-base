#include <orbis/Sysmodule.h>
#include <orbis/VideoOut.h>
#include <orbis/libkernel.h>
#include <stdint.h>

#ifndef GRAPHICS_H
#define GRAPHICS_H

#ifdef GRAPHICS_USES_FONT
#include <proto-include.h>
#endif

// Color is used to pack together RGB information, and is used for every function that draws colored
// pixels.
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class Scene2D {
#ifdef GRAPHICS_USES_FONT
    FT_Library ftLib;
#endif

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
    int width;
    int height;
    char** frameBuffers;
    int activeFrameBufferIdx;
    int frameBufferSize;
    bool initFlipQueue();
    bool allocateFrameBuffers(int num);
    char* allocateDisplayMem(size_t size);
    bool allocateVideoMem(size_t size, int alignment);
    void deallocateVideoMem();

    Scene2D(int w, int h, int pixelDepth);
    ~Scene2D();

    bool Init(size_t memSize, int numFrameBuffers);

    void SetActiveFrameBuffer(int index);
    void SubmitFlip(int frameID);

    void FrameWait(int frameID);
    void FrameBufferSwap();
    void FrameBufferClear();
    void FrameBufferFill(Color color);

    void DrawPixel(int x, int y, Color color);
    void DrawRectangle(int x, int y, int w, int h, Color color);

#ifdef GRAPHICS_USES_FONT
    bool InitFont(FT_Face* face, const char* fontPath, int fontSize);
    void DrawText(char* txt, FT_Face face, int startX, int startY, Color bgColor, Color fgColor);
    void DrawTextContainer(char* txt, FT_Face face, int startX, int startY, int maxW, int maxH,
                           Color bgColor, Color fgColor);
#endif
};

#endif
