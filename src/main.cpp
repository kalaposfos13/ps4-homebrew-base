#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#include "assert.h"
#include "logging.h"
#include "types.h"

int main(void) {
    LOG_NOTIFICATION("Hello, World!");

    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
