#include <orbis/Pad.h>
#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#include <stdio.h>
#include "assert.h"
#include "logging.h"
#include "types.h"

int main(void) {

    LOG_INFO("Starting homebrew");

    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
