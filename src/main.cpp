#include <thread>
#include <vector>
#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#include <unistd.h>
#include "assert.h"
#include "logging.h"
#include "types.h"

struct DT {
    DT() {
        thread_local int at_dtor_time = 0;
        LOG_INFO("hey world (at_dtor_time: {})", at_dtor_time);
        ++at_dtor_time;
    }
    inline static thread_local int istli;
};
DT& evil_singleton() {
    thread_local DT wow;
    return wow;
}

int main() {
    LOG_INFO("Starting homebrew");
    auto t1 = std::thread([](){
        DT& wawa1 = evil_singleton();
        DT::istli = 2;
        LOG_INFO("addr: {} istli: {} {}", fmt::ptr(&wawa1), fmt::ptr(&DT::istli), DT::istli);
    });
    DT& wawa = evil_singleton();
    DT::istli = 1;
    LOG_INFO("addrs: {} {}", fmt::ptr(&wawa), fmt::ptr(&DT::istli));
    ++DT::istli;
    LOG_INFO("istli: {}", DT::istli);
    sceSystemServiceLoadExec("EXIT", nullptr);
    return 0;
}
