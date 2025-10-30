#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#include "assert.h"
#include "logging.h"
#include "types.h"

int main(void) {
    LOG_INFO("Hello, {}!", "Klog");
    LOG_NOTIFICATION("Hello, {}!", "Screen");

    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
