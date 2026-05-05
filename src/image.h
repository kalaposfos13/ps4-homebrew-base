#pragma once

#include <vector>
#include <types.h>

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

    uint32_t* pixels{};         // mapped pointer
private:
    size_t size{};              // requested size
    size_t alignedSize{};       // actual allocated size

    u64 directMemOff{};       // handle for freeing
};