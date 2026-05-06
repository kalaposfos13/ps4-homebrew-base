#include <orbis/Sysmodule.h>

#include <orbis/_types/errors.h>
#include <orbis/_types/kernel.h>
#include <orbis/_types/user.h>
#include <orbis/_types/video.h>

#include <orbis/libkernel.h>
#include <stdint.h>

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <proto-include.h>

typedef struct OrbisVideoOutStereoBuffers {
    void* left;
    void* right;
} OrbisVideoOutStereoBuffers;

typedef struct {
    uint32_t size;
    uint8_t signalEncoding;
    uint8_t signalRange;
    uint8_t colorimetry;
    uint8_t depth;
    uint64_t refreshRate;
    uint64_t resolution;
    uint8_t contentType;
    uint8_t _reserved0[3];
    uint32_t _reserved[1];
} OrbisVideoOutMode;

extern "C" {

int32_t sceVideoOutOpen(OrbisUserServiceUserId, int32_t, int32_t, const void*);
int32_t sceVideoOutClose(int32_t);
// need to port sceVideoOutBufferAttribute (last arg)
int32_t sceVideoOutRegisterBuffers(int32_t, int32_t, void* const*, int32_t,
                                   const OrbisVideoOutBufferAttribute*);
int32_t sceVideoOutUnregisterBuffers(int32_t, int);
int32_t sceVideoOutSubmitFlip(int32_t, int32_t, uint32_t, int64_t);
void sceVideoOutSetBufferAttribute(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                   uint32_t);
int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t fliprate);
int32_t sceVideoOutAddFlipEvent(OrbisKernelEqueue, int32_t, void*);
int32_t sceVideoOutGetFlipStatus(int32_t, OrbisVideoOutFlipStatus*);
int32_t sceVideoOutIsFlipPending(int32_t);
int32_t sceVideoOutGetResolutionStatus(int32_t, OrbisVideoOutResolutionStatus* status);

void sceVideoOutConfigureOutputMode_(int32_t, uint32_t, OrbisVideoOutMode const* pMode, void*);
// void sceVideoOutGetDeviceCapabilityInfo_();
void sceVideoOutModeSetAny_(OrbisVideoOutMode*);
int sceVideoOutRegisterStereoBuffers(int32_t handle, int32_t startIndex,
                                     const OrbisVideoOutStereoBuffers* buffers, int32_t bufferNum,
                                     const OrbisVideoOutBufferAttribute* attribute);
}

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
    OrbisVideoOutStereoBuffers* frameBuffers;
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
    void DrawRectangle(int const x, int const y, int const w, int const h, Color const color);
    void DrawRectangleWithBorder(int const x, int const y, int const w, int const h,
                                 Color const color, int const b_w, Color const b_color);
    void DrawLine(int const p1x, int const p1y, int const dx, int const dy, int const w,
                  Color const c);

    bool InitFont(FT_Face* face, const char* fontPath, int fontSize);

    void DrawText(char const* txt, FT_Face face, int startX, int startY, Color bgColor,
                  Color fgColor);
};
#endif