#include "assert.h"
#include "camera.h"

Camera::Camera() {
    Init();
}

Camera::~Camera() {
    Deinit();
}

bool Camera::Init() {
    if (sceCameraIsAttached(0) != 1) {
        return false;
    }
    if (handle > 0) {
        return true;
    }

    ASSERT_NO_ERROR(handle = sceCameraOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, 0, 0, nullptr));
    LOG_INFO("PlayStation Camera connected (handle: {})", handle);

    OrbisCameraConfig cconfig{};
    cconfig.size_this = sizeof(OrbisCameraConfig);
    cconfig.config_type = ORBIS_CAMERA_CONFIG_TYPE2;
    ASSERT_OK(sceCameraSetConfig(handle, &cconfig));

    OrbisCameraStartParameter cstart_param{};
    cstart_param.size_this = sizeof(OrbisCameraStartParameter);
    cstart_param.format_level[0] = ORBIS_CAMERA_FRAME_FORMAT_LEVEL0;
    cstart_param.format_level[1] = ORBIS_CAMERA_FRAME_FORMAT_LEVEL0;
    ASSERT_OK(sceCameraStart(handle, &cstart_param));

    OrbisCameraVideoSyncParameter cvsync_param{};
    cvsync_param.size_this = sizeof(OrbisCameraVideoSyncParameter);
    cvsync_param.video_sync_mode = 1;
    ASSERT_OK(sceCameraSetVideoSync(handle, &cvsync_param));
    LOG_INFO("PlayStation Camera set up and started.");

    frame.size_this = (sizeof(OrbisCameraFrameData));
    frame.read_mode = 0;
    frame.meta.exposureGain[0].exposureControl = 0;
    frame.meta.exposureGain[1].exposureControl = 0;

    sceCameraGetExposureGain(handle, 1, &exposuregain, nullptr);

    return true;
}

void Camera::Deinit() {
    sceCameraStop(handle);
    sceCameraClose(handle);
}

bool Camera::Update() {
    if (handle == 0) {
        if (!Init()) {
            return false;
        }
    }
    if (sceCameraGetFrameData(handle, &frame) != ORBIS_OK) {
        sceKernelUsleep(10000);
        return false;
    }
    if (!sceCameraIsValidFrameData(handle, &frame)) {
        return false;
    }
    return true;
}

static inline uint32_t YUVtoRGBA(uint8_t y, uint8_t u, uint8_t v) {
    int c = (int)y - 16;
    int d = (int)u - 128;
    int e = (int)v - 128;

    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;

    r = (r < 0) ? 0 : (r > 255 ? 255 : r);
    g = (g < 0) ? 0 : (g > 255 ? 255 : g);
    b = (b < 0) ? 0 : (b > 255 ? 255 : b);

    return 0x80000000u | (r << 16) | (g << 8) | b;
}

void Camera::ConvertYUV422(const void* yuvBuffer, int w, int h, Image& out) {
    if (!out.pixels || out.width != w || out.height != h) {
        // caller responsibility: preallocate correctly
        return;
    }

    const uint8_t* src = static_cast<const uint8_t*>(yuvBuffer);
    uint32_t* dst = out.pixels;
    int stride = out.stride;

    for (int y = 0; y < h; y++) {
        uint32_t* row = dst + y * stride;
        int srcRow = y * w * 2;

        for (int x = 0; x < w; x += 2) {
            int idx = srcRow + x * 2;

            uint8_t y0 = src[idx + 0];
            uint8_t u = src[idx + 1];
            uint8_t y1 = src[idx + 2];
            uint8_t v = src[idx + 3];

            row[x + 0] = YUVtoRGBA(y0, u, v);
            row[x + 1] = YUVtoRGBA(y1, u, v);
        }
    }
}

void Camera::ConvertRAW16(const void* raw16_buf, int w, int h, Image& out) {
    if (!out.pixels || out.width != w || out.height != h) {
        return;
    }

    const uint16_t* src = static_cast<const uint16_t*>(raw16_buf);
    uint32_t* dst = out.pixels;
    int stride = out.stride;

    constexpr uint16_t WHITE = 4095;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;

            bool evenRow = (y % 2) == 0;
            bool evenCol = (x % 2) == 0;

            uint16_t R = 0, G = 0, B = 0;

            if (evenRow && evenCol) {
                B = src[idx];
                G = src[idx + 1];
                R = src[idx + w + 1];
            } else if (evenRow && !evenCol) {
                G = src[idx];
                B = src[idx - 1];
                R = src[idx + w];
            } else if (!evenRow && evenCol) {
                G = src[idx];
                R = src[idx + 1];
                B = src[idx - w];
            } else {
                R = src[idx];
                G = src[idx - 1];
                B = src[idx - w - 1];
            }

            uint8_t r = std::min<uint16_t>(WHITE, R) >> 4;
            uint8_t g = std::min<uint16_t>(WHITE, G) >> 4;
            uint8_t b = std::min<uint16_t>(WHITE, B) >> 4;

            dst[y * stride + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

bool Camera::RenderEyeToImage(int eye, int w, int h, Image& out) {
    if (handle == 0) {
        return false;
    }

    void* ptr = frame.frame_ptr_list[eye][0];
    if (!ptr) {
        LOG_ERROR("No eye data");
        return false;
    }

    auto format = frame.meta.format[eye][0];

    // ensure buffer is allocated once (no per-frame alloc)
    if (!out.pixels || out.width != w || out.height != h) {
        LOG_INFO("Realloc");
        if (!out.Allocate(w, h)) {
            return false;
        }
    }

    switch (format) {
    case ORBIS_CAMERA_FORMAT_YUV422:
        ConvertYUV422(ptr, w, h, out);
        return true;

    case ORBIS_CAMERA_FORMAT_RAW16:
        ConvertRAW16(ptr, w, h, out);
        return true;

    default:
        UNREACHABLE();
    }
}