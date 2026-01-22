// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "assert.h"
#include "logging.h"
#include <unistd.h>

#define Crash() quick_exit(1)

void assert_fail_impl() {
    std::fflush(stdout);
    usleep(10000);
    Crash();
}

[[noreturn]] void unreachable_impl() {
    std::fflush(stdout);
    usleep(10000);
    Crash();
    throw std::runtime_error("Unreachable code");
}

extern "C"
void __cxa_thread_atexit_impl() {
    // LOG_INFO("Atexit called");
}

void assert_fail_debug_msg(const char* msg) {
    LOG_CRITICAL("Assertion failed: {}", msg);
    assert_fail_impl();
}
