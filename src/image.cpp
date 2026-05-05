#include "assert.h"
#include "image.h"
#include "orbis/libkernel.h"
#include "types.h"

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

Image::~Image() {
    Free();
}

Image::Image(Image&& other) noexcept {
    *this = std::move(other);
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        Free();

        width = other.width;
        height = other.height;
        stride = other.stride;

        size = other.size;
        alignedSize = other.alignedSize;

        pixels = other.pixels;
        directMemOff = other.directMemOff;

        other.width = 0;
        other.height = 0;
        other.stride = 0;
        other.size = 0;
        other.alignedSize = 0;
        other.pixels = nullptr;
        other.directMemOff = 0;
    }
    return *this;
}

bool Image::Allocate(int w, int h) {
    Free();

    width = w;
    height = h;
    stride = w; // for now, no padding

    size = (size_t)width * height * sizeof(uint32_t);

    pixels = reinterpret_cast<u32*>(alloc_memory(size, 0, ORBIS_KERNEL_WB_ONION,
                                                 MemoryProt::CpuReadWrite | MemoryProt::GpuRead,
                                                 directMemOff));

    return true;
}

void Image::Free() {
    if (pixels) {
        // unmap is implicit on release for this usage pattern
        pixels = nullptr;
    }

    if (directMemOff) {
        sceKernelReleaseDirectMemory(directMemOff, alignedSize);
        directMemOff = 0;
    }

    width = 0;
    height = 0;
    stride = 0;
    size = 0;
    alignedSize = 0;
}