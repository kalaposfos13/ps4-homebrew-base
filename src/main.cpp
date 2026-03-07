#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#include <sys/mman.h>

#define _GNU_SOURCE 1
#include <signal.h>
#include <ucontext.h>

#include "assert.h"
#include "logging.h"
#include "signal_handler.h"
#include "types.h"

#include <atomic>

std::atomic<s32> test_value = -1;

void dummy_handler(int s) {
    LOG_INFO("Successfully caught signal {}", s);
    test_value = s;
}

void* dummy_thread_func(void*) {
    sceKernelSleep(1);
    return nullptr;
}
extern "C" int pthread_create_name_np(pthread_t* thread, const pthread_attr_t* attr,
                                      void* (*start_routine)(void*), void* arg, const char* name);

bool test_signal_availibility(s32 sig) {
    test_value = -1;
    struct sigaction act = {};
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    act.sa_sigaction = reinterpret_cast<decltype(act.sa_sigaction)>(dummy_handler);
    sigemptyset(&act.sa_mask);
    s32 ret = sigaction(sig, &act, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to add handler for signal {}: {}", sig, strerror(errno));
        return false;
    }

    pthread_t test_thread{};
    ret = pthread_create_name_np(&test_thread, nullptr, dummy_thread_func, nullptr,
                                 fmt::format("Test Thread {}", sig).c_str());
    if (ret < 0) {
        LOG_ERROR("Failed to spawn test thread: {}", sig, strerror(errno));
        return false;
    }
    ret = pthread_kill(test_thread, sig);
    if (ret < 0) {
        LOG_ERROR("Failed to send signal to test thread: {}", sig, strerror(errno));
        return false;
    }

    sceKernelUsleep(1000);

    struct sigaction act1 = {};
    act1.sa_flags = SA_SIGINFO | SA_RESTART;
    act1.sa_sigaction = nullptr;
    sigemptyset(&act1.sa_mask);
    ret = sigaction(sig, &act1, nullptr);
    if (ret < 0) {
        LOG_ERROR("Failed to remove handler for signal {}: {}", sig, strerror(errno));
        return false;
    }
    if (test_value == sig) {
        LOG_INFO("Test successful for signal {}.", sig);
        return true;
    } else {
        LOG_INFO("Test failed for signal {}, test value is {}.", sig, (s32)test_value);
        return false;
    }
}

int main(int argc, char* argv[]) {
    LOG_INFO("Starting test");

    // test_signal_availibility(128);

    int test_count = 35, successes = 0;
    for (int i = 0; i < test_count; i++) {
        LOG_INFO("Testing signal {}", i);
        successes += test_signal_availibility(i) ? 1 : 0;
        // sceKernelUsleep(1000 * 1000 * 10);
    }
    LOG_INFO("{}/{} tests successful.", successes, test_count);

    LOG_INFO("Exiting");
    sceSystemServiceLoadExec("exit", nullptr);
    return EXIT_SUCCESS;
}