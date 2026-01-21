#ifndef _SCE_CAMERA_H_
#define _SCE_CAMERA_H_

#include <orbis/UserService.h>
#include <stdint.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OrbisCameraBaseFormat {
    ORBIS_CAMERA_FORMAT_YUV422 = 0x0,
    ORBIS_CAMERA_FORMAT_RAW16,
    ORBIS_CAMERA_FORMAT_RAW8,
} OrbisCameraBaseFormat;

typedef enum OrbisCameraFrameFormatLevel {
    ORBIS_CAMERA_FRAME_FORMAT_LEVEL0 = 0x01,
    ORBIS_CAMERA_FRAME_FORMAT_LEVEL1 = 0x02,
    ORBIS_CAMERA_FRAME_FORMAT_LEVEL2 = 0x04,
    ORBIS_CAMERA_FRAME_FORMAT_LEVEL3 = 0x08,
    ORBIS_CAMERA_FRAME_FORMAT_LEVEL_ALL = 0x0F,
} OrbisCameraFrameFormatLevel;

typedef enum OrbisCameraScaleFormat {
    ORBIS_CAMERA_SCALE_FORMAT_YUV422 = 0x0,
    ORBIS_CAMERA_SCALE_FORMAT_Y16 = 0x3,
    ORBIS_CAMERA_SCALE_FORMAT_Y8,
} OrbisCameraScaleFormat;

typedef enum OrbisCameraResolution {
    ORBIS_CAMERA_RESOLUTION_1280X800 = 0x0,
    ORBIS_CAMERA_RESOLUTION_640X400,
    ORBIS_CAMERA_RESOLUTION_320X200,
    ORBIS_CAMERA_RESOLUTION_160X100,
    ORBIS_CAMERA_RESOLUTION_320X192,
    ORBIS_CAMERA_RESOLUTION_SPECIFIED_WIDTH_HEIGHT,
    ORBIS_CAMERA_RESOLUTION_UNKNOWN = 0xFF,
} OrbisCameraResolution;

typedef enum OrbisCameraFramerate {
    ORBIS_CAMERA_FRAMERATE_UNKNOWN = 0,
    ORBIS_CAMERA_FRAMERATE_7_5 = 7,
    ORBIS_CAMERA_FRAMERATE_15 = 15,
    ORBIS_CAMERA_FRAMERATE_30 = 30,
    ORBIS_CAMERA_FRAMERATE_60 = 60,
    ORBIS_CAMERA_FRAMERATE_120 = 120,
    ORBIS_CAMERA_FRAMERATE_240 = 240,
} OrbisCameraFramerate;

typedef struct OrbisCameraFormat {
    OrbisCameraBaseFormat format_level0;
    OrbisCameraScaleFormat format_level1;
    OrbisCameraScaleFormat format_level2;
    OrbisCameraScaleFormat format_level3;
} OrbisCameraFormat;

typedef enum OrbisCameraConfigType {
    ORBIS_CAMERA_CONFIG_TYPE1 = 0x01,
    ORBIS_CAMERA_CONFIG_TYPE2 = 0x02,
    ORBIS_CAMERA_CONFIG_TYPE3 = 0x03,
    ORBIS_CAMERA_CONFIG_TYPE4 = 0x04,
    ORBIS_CAMERA_CONFIG_TYPE5 = 0x05,
    ORBIS_CAMERA_CONFIG_EXTENTION = 0x10,
} OrbisCameraConfigType;

typedef struct OrbisCameraConfigExtention {
    OrbisCameraFormat format;
    OrbisCameraResolution resolution;
    OrbisCameraFramerate framerate;
    u32 width;
    u32 height;
    u32 reserved1;
    void* p_base_option;
} OrbisCameraConfigExtention;

#define ORBIS_CAMERA_MAX_DEVICE_NUM 2

typedef struct OrbisCameraConfig {
    u32 size_this;
    OrbisCameraConfigType config_type;
    OrbisCameraConfigExtention config_extention[ORBIS_CAMERA_MAX_DEVICE_NUM];
} OrbisCameraConfig;
static_assert(sizeof(OrbisCameraConfig) == 104);

typedef struct OrbisCameraStartParameter {
    u32 size_this;
    u32 format_level[ORBIS_CAMERA_MAX_DEVICE_NUM];
    void* p_start_option;
} OrbisCameraStartParameter;

// Empty Comment
void sceCameraAudioGetData();
// Empty Comment
void sceCameraAudioGetData2();
// Empty Comment
void sceCameraAudioOpen();
// Empty Comment
void sceCameraChangeAppModuleState();
// Empty Comment
void sceCameraClose(s32 handle);
// Empty Comment
void sceCameraCloseByHandle();
// Empty Comment
void sceCameraGetAttribute();
// Empty Comment
void sceCameraGetAutoExposureGain();
// Empty Comment
void sceCameraGetAutoWhiteBalance();
// Empty Comment
void sceCameraGetCalibrationData();
// Empty Comment
void sceCameraGetConfig();
// Empty Comment
void sceCameraGetContrast();
// Empty Comment
void sceCameraGetDefectivePixelCancellation();
// Empty Comment
void sceCameraGetDeviceConfig();
// Empty Comment
void sceCameraGetDeviceInfo();
// Empty Comment
void sceCameraGetExposureGain();
// Empty Comment
void sceCameraGetFrameData();
// Empty Comment
void sceCameraGetGamma();
// Empty Comment
void sceCameraGetHue();
// Empty Comment
void sceCameraGetLensCorrection();
// Empty Comment
void sceCameraGetSaturation();
// Empty Comment
void sceCameraGetSharpness();
// Empty Comment
void sceCameraGetWhiteBalance();
// Empty Comment
s32 sceCameraIsAttached(s32 i);
// Empty Comment
void sceCameraIsValidFrameData();
// Empty Comment
int sceCameraOpen(OrbisUserServiceUserId user_id, s32 type, s32 index, void* r1);
// Empty Comment
void sceCameraOpenByModuleId();
// Empty Comment
void sceCameraRemoveAppModuleFocus();
// Empty Comment
void sceCameraSetAppModuleFocus();
// Empty Comment
void sceCameraSetAttribute();
// Empty Comment
void sceCameraSetAttributeInternal();
// Empty Comment
void sceCameraSetAutoExposureGain();
// Empty Comment
void sceCameraSetAutoWhiteBalance();
// Empty Comment
void sceCameraSetCalibData();
// Empty Comment
int sceCameraSetConfig(int32_t handle, OrbisCameraConfig* pConfig);
// Empty Comment
void sceCameraSetConfigInternal();
// Empty Comment
void sceCameraSetContrast();
// Empty Comment
void sceCameraSetDebugStop();
// Empty Comment
void sceCameraSetDefectivePixelCancellation();
// Empty Comment
void sceCameraSetDefectivePixelCancellationInternal();
// Empty Comment
void sceCameraSetExposureGain();
// Empty Comment
void sceCameraSetForceActivate();
// Empty Comment
void sceCameraSetGamma();
// Empty Comment
void sceCameraSetHue();
// Empty Comment
void sceCameraSetLensCorrection();
// Empty Comment
void sceCameraSetLensCorrectionInternal();
// Empty Comment
void sceCameraSetProcessFocusByHandle();
// Empty Comment
void sceCameraSetSaturation();
// Empty Comment
void sceCameraSetSharpness();
// Empty Comment
void sceCameraSetVideoSync();
// Empty Comment
void sceCameraSetVideoSyncInternal();
// Empty Comment
void sceCameraSetWhiteBalance();
// Empty Comment
int sceCameraStart(s32 handle, OrbisCameraStartParameter* pParam);
// Empty Comment
void sceCameraStartByHandle();
// Empty Comment
void sceCameraStop(s32 handle);

#ifdef __cplusplus
}
#endif

#endif
