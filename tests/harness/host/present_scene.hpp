#ifndef AV_PRESENT_SCENE_HPP
#define AV_PRESENT_SCENE_HPP

#include "harness/host/gpu_request_builder.hpp"

#include <cstdint>

namespace av {

class MockHost;
class PresentProgress;

class PresentScene {
public:
    static constexpr std::uint64_t ram = 0x100000u;
    static constexpr std::uint64_t page_bytes = 0x1000u;
    static constexpr std::uint64_t ram_bytes = 16u * page_bytes;
    static constexpr std::uint64_t engine_page = ram + 12u * page_bytes;
    static constexpr std::uint32_t width = 16u;
    static constexpr std::uint32_t height = 128u;
    static constexpr std::uint32_t pitch = width * 4u;

    explicit PresentScene(MockHost &host);
    PresentScene(const PresentScene &) = delete;
    PresentScene &operator=(const PresentScene &) = delete;

    std::uint32_t source_pixel(std::uint32_t x, std::uint32_t y) const;
    std::uint32_t destination_pixel(std::uint32_t x, std::uint32_t y) const;
    bool          copied(std::uint32_t first, std::uint32_t rows) const;
    GpuRequestBuilder blt(std::uint64_t ticket, std::uint32_t first, std::uint32_t rows) const;
    GpuRequestBuilder fill(std::uint64_t ticket, std::uint32_t colour, std::uint32_t first, std::uint32_t rows) const;
    GpuRequestBuilder image(std::uint64_t ticket, std::uint64_t key) const;
    GpuRequestBuilder surfaces(std::uint64_t ticket, std::uint64_t source, std::uint64_t destination,
                               std::uint32_t regions = 1u) const;
    GpuRequestBuilder texture(std::uint64_t ticket, std::uint64_t key) const;
    bool              progress(PresentProgress &progress) const;

private:
    std::uint8_t *source(std::uint32_t x, std::uint32_t y) const;
    std::uint8_t *destination(std::uint32_t x, std::uint32_t y) const;

    MockHost &host_;
};

}

#endif
