#include "av_player.h"
#include "orbis/libkernel.h"
#include "unordered_map"
#include "mutex"
#include "logging.h"

enum MemoryProt : u32 {
    NoAccess = 0,
    CpuRead = 1,
    CpuWrite = 2,
    CpuReadWrite = 3,
    CpuExec = 4,
    GpuRead = 16,
    GpuWrite = 32,
    GpuReadWrite = 48,
};
template <typename T>
[[nodiscard]] constexpr T AlignUp(T value, std::size_t size) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    auto mod{static_cast<T>(value % size)};
    value -= mod;
    return static_cast<T>(mod == T{0} ? value : value + size);
}

static std::unordered_map<void*, std::size_t> g_flexmap;
static std::mutex g_flexmap_mutex;

void* av_allocate(void* handle, u32 alignment, u32 size) {
    LOG_INFO("called, size: {:#x}, alignment: {:#x}", size, alignment);
    void* out = 0;
    const std::size_t mapped_size = AlignUp(static_cast<u64>(size), 16_KB);
    s32 ret = sceKernelMapFlexibleMemory(&out, mapped_size,
                                         MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite, 0);
    if (ret != 0) {
        LOG_ERROR("sceKernelMapFlexibleMemory failed: {:#x}", ret);
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lk(g_flexmap_mutex);
        g_flexmap[(void*)out] = mapped_size;
    }
    return (void*)out;
}

void av_deallocate(void* handle, void* memory) {
    LOG_INFO("called");
    if (!memory)
        return;
    std::size_t mapped_size = 0;
    {
        std::lock_guard<std::mutex> lk(g_flexmap_mutex);
        auto it = g_flexmap.find(memory);
        if (it != g_flexmap.end()) {
            mapped_size = it->second;
            g_flexmap.erase(it);
        } else {
            LOG_WARNING("av_deallocate: unknown pointer, falling back to 16_KB");
            mapped_size = 16_KB;
        }
    }
    sceKernelReleaseFlexibleMemory(memory, mapped_size);
    return;
}

void* av_allocate_texture(void* handle, u32 alignment, u32 size) {
    // LOG_INFO("called, size: {:#x}, alignment: {:#x}", size, alignment);
    void* out = 0;
    const std::size_t mapped_size = AlignUp(static_cast<u64>(size), 16_KB);
    s32 ret = sceKernelMapFlexibleMemory(&out, mapped_size,
                                         MemoryProt::CpuReadWrite | MemoryProt::GpuReadWrite, 0);
    if (ret != 0) {
        LOG_ERROR("sceKernelMapFlexibleMemory (texture) failed: {:#x}", ret);
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lk(g_flexmap_mutex);
        g_flexmap[(void*)out] = mapped_size;
    }
    LOG_DEBUG("Allocated {:#x} bytes of memory to {}", mapped_size, (void*)out);
    return (void*)out;
}

void av_deallocate_texture(void* handle, void* memory) {
    // LOG_INFO("called");
    if (!memory)
        return;
    std::size_t mapped_size = 0;
    {
        std::lock_guard<std::mutex> lk(g_flexmap_mutex);
        auto it = g_flexmap.find(memory);
        if (it != g_flexmap.end()) {
            mapped_size = it->second;
            g_flexmap.erase(it);
        } else {
            LOG_WARNING("av_deallocate_texture: unknown pointer, falling back to 16_KB");
            mapped_size = 16_KB;
        }
    }
    sceKernelReleaseFlexibleMemory(memory, mapped_size);
    LOG_DEBUG("Released {:#x} bytes of memory from {}", mapped_size, memory);
    return;
}
