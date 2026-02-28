#ifndef _ORBIS_CAMERA_H_
#define _ORBIS_CAMERA_H_

#include <orbis/UserService.h>
#include <stdint.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OrbisFVector2 {
    float x, y;
} OrbisFVector2;
typedef struct OrbisFVector3 {
    float x, y, z;
} OrbisFVector3;
typedef struct OrbisFVector4 {
    float x, y, z, w;
} OrbisFVector4;

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

typedef enum OrbisCameraFrameMemoryType {
    ORBIS_CAMERA_FRAME_MEMORY_TYPE_ONION = 0x0000,
    ORBIS_CAMERA_FRAME_MEMORY_TYPE_GARLIC = 0x0001,
} OrbisCameraFrameMemoryType;

typedef enum OrbisCameraFrameWaitNextFrameMode {
    ORBIS_CAMERA_FRAME_WAIT_NEXTFRAME_ON_MODE_0 = 0x0000,
    ORBIS_CAMERA_FRAME_WAIT_NEXTFRAME_OFF_MODE_0 = 0x0010,
    ORBIS_CAMERA_FRAME_WAIT_NEXTFRAME_ON_MODE_1 = 0x0020,
    ORBIS_CAMERA_FRAME_WAIT_NEXTFRAME_OFF_MODE_1 = 0x0030,
} OrbisCameraFrameWaitNextFrameMode;

typedef enum OrbisCameraStatus {
    ORBIS_CAMERA_STATUS_IS_INVALID_META_DATA = -2,
    ORBIS_CAMERA_STATUS_IS_INVALID_FRAME = -1,
    ORBIS_CAMERA_STATUS_IS_NOT_ACTIVE = 0,
    ORBIS_CAMERA_STATUS_IS_ACTIVE = 1,
    ORBIS_CAMERA_STATUS_IS_ALREADY_READ = 2,
    ORBIS_CAMERA_STATUS_IS_NOT_STABLE = 3,
    ORBIS_CAMERA_STATUS_IS_SYNC_ADJUSTING = 4,
} OrbisCameraStatus;

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

typedef enum OrbisCameraChannel {
    SCE_CAMERA_CHANNEL_0 = 1,
    SCE_CAMERA_CHANNEL_1 = 2,
    SCE_CAMERA_CHANNEL_BOTH = 3,
} OrbisCameraChannel;

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
#define ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM 4

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

typedef struct OrbisCameraVideoSyncParameter {
    u32 size_this;
    u32 video_sync_mode;
    void* p_mode_option;
} OrbisCameraVideoSyncParameter;

typedef struct OrbisCameraFramePosition {
    u32 x;
    u32 y;
    u32 x_size;
    u32 y_size;
} OrbisCameraFramePosition;

typedef struct OrbisCameraExposureGain {
    u32 exposureControl;
    u32 exposure;
    u32 gain;
    u32 mode;
} OrbisCameraExposureGain;

typedef struct OrbisCameraWhiteBalance {
    u32 whiteBalanceControl;
    u32 gainRed;
    u32 gainBlue;
    u32 gainGreen;
} OrbisCameraWhiteBalance;

typedef struct OrbisCameraGamma {
    u32 gammaControl;
    u32 value;
    u8 reserved[16];
} OrbisCameraGamma;

typedef struct OrbisCameraMeta {
    u32 metaMode;
    u32 format[ORBIS_CAMERA_MAX_DEVICE_NUM][ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
    u64 frame[ORBIS_CAMERA_MAX_DEVICE_NUM];
    u64 timestamp[ORBIS_CAMERA_MAX_DEVICE_NUM];
    u32 deviceTimestamp[ORBIS_CAMERA_MAX_DEVICE_NUM];
    OrbisCameraExposureGain exposureGain[ORBIS_CAMERA_MAX_DEVICE_NUM];
    OrbisCameraWhiteBalance whiteBalance[ORBIS_CAMERA_MAX_DEVICE_NUM];
    OrbisCameraGamma gamma[ORBIS_CAMERA_MAX_DEVICE_NUM];
    u32 luminance[ORBIS_CAMERA_MAX_DEVICE_NUM];
    OrbisFVector3 acceleration;
    u64 vcounter;
    u32 reserved[16];
} OrbisCameraMeta;

typedef struct OrbisCameraFrameData {
    u32 size_this;
    u32 read_mode;
    OrbisCameraFramePosition frame_position[ORBIS_CAMERA_MAX_DEVICE_NUM]
                                           [ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
    void* frame_ptr_list[ORBIS_CAMERA_MAX_DEVICE_NUM][ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
    u32 frame_size[ORBIS_CAMERA_MAX_DEVICE_NUM][ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
    u32 status[ORBIS_CAMERA_MAX_DEVICE_NUM];
    OrbisCameraMeta meta;
    // void* frame_ptr_list_garlic[ORBIS_CAMERA_MAX_DEVICE_NUM][ORBIS_CAMERA_MAX_FORMAT_LEVEL_NUM];
} OrbisCameraFrameData;

// from the playroom, but this just doesn't want to line up correctly
// static_assert(sizeof(OrbisCameraFrameData) == 520);

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
s32 sceCameraGetExposureGain(int32_t handle, s32 channel, OrbisCameraExposureGain* pExposureGain,
                             void* pOption);
// Empty Comment
s32 sceCameraGetFrameData(s32 handle, OrbisCameraFrameData* pFrameData);
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
s32 sceCameraIsValidFrameData(s32 handle, OrbisCameraFrameData* pFrameData);
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
int sceCameraSetVideoSync(int32_t handle, OrbisCameraVideoSyncParameter* pVideoSync);
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
