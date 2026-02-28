#pragma once

#include "types.h"

extern "C" {

constexpr s32 ORBIS_PAD_TRACKER_CONTROLLER_MAX = 4;
constexpr s32 ORBIS_PAD_TRACKER_IMAGE_MAX = 2;
constexpr s32 ORBIS_PAD_TRACKER_ERROR_ALREADY_INIT = 0x80C40002;
constexpr s32 ORBIS_PAD_TRACKER_ERROR_INVALID_ARG = 0x80C40003;
constexpr s32 ORBIS_PAD_TRACKER_ERROR_INVALID_HANDLE = 0x80C40004;
constexpr s32 ORBIS_PAD_TRACKER_ERROR_NOT_INIT = 0x80C40001;

enum OrbisPadTrackerStatus {
    ORBIS_PAD_TRACKER_TRACKING = 0,
    ORBIS_PAD_TRACKER_NOT_TRACKING = 1,
    ORBIS_PAD_TRACKER_ROOM_CONFLICT = 2,
    ORBIS_PAD_TRACKER_CALIBRATING = 3
};

struct OrbisPadTrackerImageCoordinates {
    OrbisPadTrackerStatus status;
    float x;
    float y;
};

struct OrbisPadTrackerData {
    OrbisPadTrackerImageCoordinates imageCoordinates[ORBIS_PAD_TRACKER_IMAGE_MAX];
};

struct OrbisPadTrackerImage {
    int exposure;
    int gain;
    int width;
    int height;
    void* data;
};

struct OrbisPadTrackerInput {
    s32 handles[ORBIS_PAD_TRACKER_CONTROLLER_MAX];
    OrbisPadTrackerImage images[ORBIS_PAD_TRACKER_IMAGE_MAX];
};

s32 scePadTrackerCalibrate();
s32 scePadTrackerGetWorkingMemorySize(s32* onionSize, s32* garlicSize);
s32 scePadTrackerInit(void* onionMemory, void* garlicMemory, int pipeId, int queueId);
s32 scePadTrackerReadState(s32 handle, OrbisPadTrackerData* data);
s32 scePadTrackerTerm();
s32 scePadTrackerUpdate(OrbisPadTrackerInput input);

}