#pragma once

#include <vector>
#include <types.h>

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
static VAddr alloc_memory(size_t size, size_t alignment, u32 memType, u32 prot, u64& out_dmem_off) {
    size = AlignUp(size, 16_KB);
    ASSERT(alignment == AlignUp(alignment, 16_KB));

    off_t phys = 0;
    ASSERT_OK(sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), size, alignment,
                                            memType, &phys));

    void* virt = nullptr;
    ASSERT_OK(sceKernelMapDirectMemory(&virt, size, prot, 0, phys, alignment));

    return VAddr(virt);
}

class Image {
public:
    Image() = default;
    ~Image();

    // non-copyable (owns direct memory)
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    // movable
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    bool Allocate(int w, int h);
    void Free();

    int width{};
    int height{};
    int stride{};

    uint32_t* pixels{}; // mapped pointer
private:
    size_t size{};        // requested size
    size_t alignedSize{}; // actual allocated size

    u64 directMemOff{}; // handle for freeing
};