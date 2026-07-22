#include "assert.h"
#include "image.h"
#include "orbis/libkernel.h"
#include "types.h"

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