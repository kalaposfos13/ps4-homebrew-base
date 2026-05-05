#include "types.h"
#include <orbis/UserService.h>

extern "C" {

typedef struct OrbisHmdInitializeParam {
    void* reserved0;
    uint8_t reserved[8];
} OrbisHmdInitializeParam;

typedef struct OrbisHmdDeviceInformation {
	uint32_t status;
	OrbisUserServiceUserId userId;
	uint8_t reserve0[4];
	struct DeviceInfo {
		struct PanelResolution {
			uint32_t width;
			uint32_t height;
		} panelResolution;
		struct FlipToDisplayLatency {
			uint16_t refreshRate90Hz;
			uint16_t refreshRate120Hz;
		} flipToDisplayLatency;
	} deviceInfo;
	uint8_t hmuMount;
	uint8_t reserve1[7];
} OrbisHmdDeviceInformation;

int32_t sceHmdInitialize(const OrbisHmdInitializeParam* param);
int32_t sceHmdGetDeviceInformation(
	OrbisHmdDeviceInformation *info
);
int sceHmdTerminate();
}