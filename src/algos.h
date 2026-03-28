#pragma once

#include <cmath>
#include "types.h"
#include "graphics.h"
#include "camera.h"

static inline uint32_t YUVtoRGBA(uint8_t y, uint8_t u, uint8_t v) {
    int c = (int)y - 16;
    int d = (int)u - 128;
    int e = (int)v - 128;

    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;

    if (r < 0)
        r = 0;
    else if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    else if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    else if (b > 255)
        b = 255;

    return 0x80000000u | (r << 16) | (g << 8) | b;
}
static void DrawYUV422Frame(Scene2D* scene, void* yuvBuffer, int width, int height) {
    u8* src = static_cast<u8*>(yuvBuffer);
    u32* dst = reinterpret_cast<u32*>(scene->frameBuffers[scene->activeFrameBufferIdx]);

    for (int y = 0; y < height; y++) {
        u32* row = dst + y * scene->width;
        int srcRow = y * width * 2;

        for (int x = 0; x < width; x += 2) {
            int idx = srcRow + x * 2;

            u16 y0 = src[idx + 0];
            u16 u = src[idx + 1];
            u16 y1 = src[idx + 2];
            u16 v = src[idx + 3];

            row[x + 0] = YUVtoRGBA(y0, u, v);
            row[x + 1] = YUVtoRGBA(y1, u, v);
        }
    }
}
void DrawRAW16Frame(Scene2D* scene, void* rawBuffer, int width, int height) {
    const u16* src = static_cast<const u16*>(rawBuffer);
    u32* dst = reinterpret_cast<u32*>(scene->frameBuffers[scene->activeFrameBufferIdx]);
    static s32 start_pixel_offset = 0;
    for (int y = start_pixel_offset; y < height - 1; y += 3) {
        for (int x = start_pixel_offset; x < width - 1; x += 3) {
            int idx = y * width + x;

            bool evenRow = (y % 2) == 0;
            bool evenCol = (x % 2) == 0;

            u16 R = 0, G = 0, B = 0;

            if (evenRow && evenCol) {
                // B
                B = src[idx];
                G = src[idx + 1];
                R = src[idx + width + 1];
            } else if (evenRow && !evenCol) {
                // G (blue row)
                G = src[idx];
                B = src[idx - 1];
                R = src[idx + width];
            } else if (!evenRow && evenCol) {
                // G (red row)
                G = src[idx];
                R = src[idx + 1];
                B = src[idx - width];
            } else if (!evenRow && !evenCol) {
                // R
                R = src[idx];
                G = src[idx - 1];
                B = src[idx - width - 1];
            }

            constexpr u16 WHITE = 4095;

            u8 r = std::min(WHITE, R) >> 4;
            u8 g = std::min(WHITE, G) >> 4;
            u8 b = std::min(WHITE, B) >> 4;

            dst[y * scene->width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
    // start_pixel_offset = (start_pixel_offset + 1) % 4;
}

static void rotate_x(float* y, float* z, float angle) {
    float cy = cosf(angle);
    float sy = sinf(angle);

    float ny = (*y) * cy - (*z) * sy;
    float nz = (*y) * sy + (*z) * cy;

    *y = ny;
    *z = nz;
}

static void rotate_z(float* x, float* y, float angle) {
    float cx = cosf(angle);
    float sx = sinf(angle);

    float nx = (*x) * cx - (*y) * sx;
    float ny = (*x) * sx + (*y) * cx;

    *x = nx;
    *y = ny;
}

static void quat_rotate(OrbisFQuaternion const q, float v[3]) {
    float x = v[0], y = v[1], z = v[2];

    float qx = q.x;
    float qy = q.y;
    float qz = q.z;
    float qw = q.w;

    float ix = qw * x + qy * z - qz * y;
    float iy = qw * y + qz * x - qx * z;
    float iz = qw * z + qx * y - qy * x;
    float iw = -qx * x - qy * y - qz * z;

    v[0] = ix * qw + iw * -qx + iy * -qz - iz * -qy;
    v[1] = iy * qw + iw * -qy + iz * -qx - ix * -qz;
    v[2] = iz * qw + iw * -qz + ix * -qy - iy * -qx;
}