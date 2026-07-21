#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "graphics.h"
#include "logging.h"

Scene2D::Scene2D(int w, int h, int pixelDepth)
    : width(w), height(h), depth(pixelDepth),
      frameBufferSize(static_cast<size_t>(w) * static_cast<size_t>(h) *
                      static_cast<size_t>(pixelDepth)),
      frameBuffers(nullptr), activeFrameBufferIdx(0), video(0), videoMem(nullptr), videoMemSP(0),
      directMemOff(0), directMemAllocationSize(0) {}

Scene2D::~Scene2D() {
    deallocateVideoMem();
    if (ftLib) {
        FT_Done_FreeType(ftLib);
    }
}

bool Scene2D::Init(size_t memSize, int numFrameBuffers) {
    int rc;

    this->video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);
    this->videoMem = NULL;

    if (this->video < 0) {
        LOG_DEBUG("Failed to open a video out handle: {}", std::string(strerror(errno)));
        return false;
    }

    if (!initFlipQueue()) {
        LOG_DEBUG("Failed to initialize flip queue: {}", std::string(strerror(errno)));
        return false;
    }

    if (!allocateVideoMem(memSize, 0x200000)) {
        LOG_DEBUG("Failed to allocate video memory: {}", std::string(strerror(errno)));
        return false;
    }

    if (!allocateFrameBuffers(numFrameBuffers)) {
        LOG_DEBUG("Failed to allocate frame buffers: {}", std::string(strerror(errno)));
        return false;
    }

    sceVideoOutSetFlipRate(this->video, 0);
    return true;
}

int Scene2D::InitFontLib() {
    // Load freetype
    int rc = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);
    if (rc < 0) {
        LOG_DEBUG("Failed to load freetype: {}", std::string(strerror(errno)));
        return ORBIS_FAIL;
    }
    // Initialize freetype
    rc = FT_Init_FreeType(&this->ftLib);

    if (rc != 0) {
        LOG_DEBUG("Failed to initialize freetype: {}", std::string(strerror(errno)));
        return ORBIS_FAIL;
    }
    return ORBIS_OK;
}

bool Scene2D::initFlipQueue() {
    int rc = sceKernelCreateEqueue(&flipQueue, "homebrew flip queue");

    if (rc < 0)
        return false;

    sceVideoOutAddFlipEvent(flipQueue, this->video, 0);
    return true;
}

bool Scene2D::allocateFrameBuffers(int num) {
    // Allocate frame buffers array
    this->frameBuffers = new char*[num]{0};

    // Set the display buffers
    for (int i = 0; i < num; i++) {
        this->frameBuffers[i] = this->allocateDisplayMem(frameBufferSize);
    }

    // Set SRGB pixel format
    sceVideoOutSetBufferAttribute(&this->attr, 0x80000000, 1, 0, this->width, this->height,
                                  this->width);

    // Register the buffers to the video handle
    return (sceVideoOutRegisterBuffers(this->video, 0, (void**)this->frameBuffers, num,
                                       &this->attr) == 0);
}

char* Scene2D::allocateDisplayMem(size_t size) {
    // Essentially just bump allocation
    char* allocatedPtr = (char*)videoMemSP;
    videoMemSP += size;

    return allocatedPtr;
}

bool Scene2D::allocateVideoMem(size_t size, int alignment) {
    int rc;

    // Align the allocation size
    this->directMemAllocationSize = (size + alignment - 1) / alignment * alignment;

    // Allocate memory for display buffer
    rc = sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(),
                                       this->directMemAllocationSize, alignment, 3,
                                       &this->directMemOff);

    if (rc < 0) {
        this->directMemAllocationSize = 0;
        return false;
    }

    // Map the direct memory
    rc = sceKernelMapDirectMemory(&this->videoMem, this->directMemAllocationSize, 0x33, 0,
                                  this->directMemOff, alignment);

    if (rc < 0) {
        sceKernelReleaseDirectMemory(this->directMemOff, this->directMemAllocationSize);

        this->directMemOff = 0;
        this->directMemAllocationSize = 0;

        return false;
    }

    // Set the stack pointer to the beginning of the buffer
    this->videoMemSP = (uintptr_t)this->videoMem;
    return true;
}

void Scene2D::deallocateVideoMem() {
    // Free the direct memory
    if (this->directMemOff)
        sceKernelReleaseDirectMemory(this->directMemOff, this->directMemAllocationSize);

    // Zero out metadata
    this->videoMem = nullptr;
    this->videoMemSP = 0;
    this->directMemOff = 0;
    this->directMemAllocationSize = 0;

    // Free the frame buffer array (delete[] because it was new char*[])
    if (this->frameBuffers) {
        delete[] this->frameBuffers;
        this->frameBuffers = nullptr;
    }
    this->activeFrameBufferIdx = 0;
}

void Scene2D::SetActiveFrameBuffer(int index) {
    this->activeFrameBufferIdx = index;
}

void Scene2D::SubmitFlip() {
    sceVideoOutSubmitFlip(this->video, this->activeFrameBufferIdx, ORBIS_VIDEO_OUT_FLIP_VSYNC,
                          frame_id);
}

void Scene2D::FrameWait() {
    OrbisKernelEvent evt;
    int count;

    // If the video handle is not initialized, bail out. This is mostly a failsafe, this should
    // never happen.
    if (this->video == 0)
        return;

    for (;;) {
        OrbisVideoOutFlipStatus flipStatus;

        // Get the flip status and check the arg for the given frame ID
        sceVideoOutGetFlipStatus(video, &flipStatus);

        if (flipStatus.flipArg == frame_id)
            break;

        // Wait on next flip event
        if (sceKernelWaitEqueue(this->flipQueue, &evt, 1, &count, 0) != 0)
            break;
    }
    frame_id++;
}

void Scene2D::FrameBufferSwap() {
    // Swap the frame buffer for some perf
    this->activeFrameBufferIdx = 1 - this->activeFrameBufferIdx;
}

void Scene2D::FrameBufferClear() {
    // Clear the screen with a uniform colour
    Color blank = {50, 50, 50};
    FrameBufferFill(blank);
}

bool Scene2D::InitFont(FT_Face* face, const char* fontPath, int fontSize) {
    int rc;

    rc = FT_New_Face(this->ftLib, fontPath, 0, face);

    if (rc < 0)
        return false;

    rc = FT_Set_Pixel_Sizes(*face, 0, fontSize);

    if (rc < 0)
        return false;

    return true;
}

void Scene2D::FrameBufferFill(Color color) {
    DrawRectangle(0, 0, this->width, this->height, color);
}

void Scene2D::DrawPixel(int const x, int const y, Color const color) {
    if (x < 0 || y < 0 || x > width || y > height) {
        return;
    }
    int pixel = (y * this->width) + x;
    uint32_t encodedColor = 0x80000000u | (color.r << 16) | (color.g << 8) | color.b;
    ((uint32_t*)this->frameBuffers[this->activeFrameBufferIdx])[pixel] = encodedColor;
}

void Scene2D::DrawPixel(int const x, int const y, int const color) {
    if (x < 0 || y < 0 || x > width || y > height) {
        return;
    }
    int pixel = (y * this->width) + x;
    ((uint32_t*)this->frameBuffers[this->activeFrameBufferIdx])[pixel] = color;
}

void Scene2D::DrawRectangle(int const x, int const y, int const w, int const h, Color const color) {
    int xPos, yPos;
    if (x < 0 || y < 0 || x + w > width || y + h > height) {
        return;
    }

    // Draw row-by-row, column-by-column
    for (yPos = y; yPos < y + h; yPos++) {
        for (xPos = x; xPos < x + w; xPos++) {
            DrawPixel(xPos, yPos, color);
        }
    }
}

void Scene2D::DrawRectangle(int const x, int const y, int const w, int const h, int const color) {
    int xPos, yPos;
    if (x < 0 || y < 0 || x + w > width || y + h > height) {
        return;
    }

    // Draw row-by-row, column-by-column
    for (yPos = y; yPos < y + h; yPos++) {
        for (xPos = x; xPos < x + w; xPos++) {
            DrawPixel(xPos, yPos, color);
        }
    }
}

void Scene2D::DrawRectangleWithBorder(int const x, int const y, int const w, int const h,
                                      Color const color, int const b_w, Color const b_color) {
    int xPos, yPos;
    // top
    for (yPos = y; yPos < y + b_w; yPos++) {
        for (xPos = x; xPos < x + w; xPos++) {
            DrawPixel(xPos, yPos, b_color);
        }
    }
    // bottom
    for (yPos = y + h - b_w; yPos < y + h; yPos++) {
        for (xPos = x; xPos < x + w; xPos++) {
            DrawPixel(xPos, yPos, b_color);
        }
    }
    // left
    for (yPos = y + b_w; yPos < y + h - b_w; yPos++) {
        for (xPos = x; xPos < x + b_w; xPos++) {
            DrawPixel(xPos, yPos, b_color);
        }
    }
    // right
    for (yPos = y + b_w; yPos < y + h - b_w; yPos++) {
        for (xPos = x + w - b_w; xPos < x + w; xPos++) {
            DrawPixel(xPos, yPos, b_color);
        }
    }
    // center
    for (yPos = y + b_w; yPos < y + h - b_w; yPos++) {
        for (xPos = x + b_w; xPos < x + w - b_w; xPos++) {
            DrawPixel(xPos, yPos, color);
        }
    }
}

void Scene2D::DrawLine(int const p1x, int const p1y, int const dx, int const dy, int const w,
                       Color const c) {

    int p2x = p1x + dx;
    int p2y = p1y + dy;
    int adx = abs(dx);
    int ady = -abs(dy);

    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;

    int err = adx + ady;

    int x = p1x;
    int y = p1y;

    while (true) {
        for (int oy = -w / 2; oy <= w / 2; ++oy) {
            for (int ox = -w / 2; ox <= w / 2; ++ox) {
                DrawPixel(x + ox, y + oy, c);
            }
        }

        if (x == p2x && y == p2y)
            break;

        int e2 = 2 * err;

        if (e2 >= ady) {
            err += ady;
            x += sx;
        }
        if (e2 <= adx) {
            err += adx;
            y += sy;
        }
    }
}

void Scene2D::DrawText(char const* txt, FT_Face face, int startX, int startY, Color bgColor,
                       Color fgColor) {
    int rc;
    int xOffset = 0;
    int yOffset = 0;

    // Get the glyph slot for bitmap and font metrics
    FT_GlyphSlot slot = face->glyph;

    // Iterate each character of the text to write to the screen
    for (int n = 0; n < strlen(txt); n++) {
        FT_UInt glyph_index;

        // Get the glyph for the ASCII code
        glyph_index = FT_Get_Char_Index(face, txt[n]);

        // Load and render in 8-bit color
        rc = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);

        if (rc)
            continue;

        rc = FT_Render_Glyph(slot, ft_render_mode_normal);

        if (rc)
            continue;

        // If we get a newline, increment the y offset, reset the x offset, and skip to the next
        // character
        if (txt[n] == '\n') {
            xOffset = 0;
            yOffset += 50;

            continue;
        }

        // Parse and write the bitmap to the frame buffer
        for (int yPos = 0; yPos < slot->bitmap.rows; yPos++) {
            for (int xPos = 0; xPos < slot->bitmap.width; xPos++) {
                // Decode the 8-bit bitmap
                char pixel = slot->bitmap.buffer[(yPos * slot->bitmap.width) + xPos];

                // Get new pixel coordinates to account for the character position and baseline, as
                // well as newlines
                int x = startX + xPos + xOffset + slot->bitmap_left;
                int y = startY + yPos + yOffset - slot->bitmap_top;

                // Linearly interpolate between the foreground and background for smoother rendering
                uint8_t r = (pixel * fgColor.r) / 255;
                uint8_t g = (pixel * fgColor.g) / 255;
                uint8_t b = (pixel * fgColor.b) / 255;

                // Create new color struct with lerp'd values
                Color finalColor = {r, g, b};

                // We need to do bounds checking before commiting the pixel write due to our
                // transformations, or we could write out-of-bounds of the frame buffer
                if (x < 0 || y < 0 || x >= this->width || y >= this->height)
                    continue;

                // If the pixel in the bitmap isn't blank, we'll draw it
                if (pixel != 0x00)
                    this->DrawPixel(x, y, finalColor);
            }
        }

        // Increment x offset for the next character
        xOffset += slot->advance.x >> 6;
    }
}
