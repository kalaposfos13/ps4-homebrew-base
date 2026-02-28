#pragma once

#include "types.h"

extern "C" {
int PS4_SYSV_ABI sceGnmMapComputeQueue(u32 pipe_id, u32 queue_id, VAddr ring_base_addr,
                                       u32 ring_size_dw, u32* read_ptr_addr);
}