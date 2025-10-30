#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#include "assert.h"
#include "logging.h"
#include "types.h"

#include "time.h"


int main(void) {
    LOG_INFO("Starting homebrew");

    struct timespec ts;
    int ret = clock_gettime(8, &ts);
    if (ret != 0) {
        LOG_ERROR("clock_gettime returned {}", errno);
        return 1;
    }

    time_t total = ts.tv_sec;
    int days    = total / 86400;
    int hours   = (total % 86400) / 3600;
    int minutes = (total % 3600) / 60;
    int seconds = total % 60;

    std::string uptime = fmt::format("{} days {} hours {} minutes {} seconds", days, hours, minutes, seconds);

    LOG_INFO("Uptime: {} ({} seconds total)\n", total, uptime);
    LOG_NOTIFICATION("Uptime: {}", uptime);

    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
