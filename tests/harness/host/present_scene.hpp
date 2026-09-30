/* Guest RAM laid out for the present engine's tests: the engine's page, a
 * source surface whose two pages are apart and out of order, and a
 * destination of one run, 16 x 128 pixels each - and the queued presents
 * that name them, as the miniport writes them (docs/protocol.md, "Present
 * engine"). */
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

    /* Adds the scene's RAM to `host` and numbers every source pixel apart. */
    explicit PresentScene(MockHost &host);
    PresentScene(const PresentScene &) = delete;
    PresentScene &operator=(const PresentScene &) = delete;

    std::uint32_t source_pixel(std::uint32_t x, std::uint32_t y) const;
    std::uint32_t destination_pixel(std::uint32_t x, std::uint32_t y) const;
    /* Whether rows [first, first + rows) of the destination are the source's. */
    bool          copied(std::uint32_t first, std::uint32_t rows) const;
    /* A blt under `ticket` of rows [first, first + rows) onto the same rows. */
    GpuRequestBuilder blt(std::uint64_t ticket, std::uint32_t first, std::uint32_t rows) const;
    /* A fill under `ticket` of `colour` over rows [first, first + rows). */
    GpuRequestBuilder fill(std::uint64_t ticket, std::uint32_t colour, std::uint32_t first, std::uint32_t rows) const;
    /* A blt under `ticket` from the image shared under `key`. */
    GpuRequestBuilder image(std::uint64_t ticket, std::uint64_t key) const;
    /* What the engine's page says: false when it does not decode. */
    bool              progress(PresentProgress &progress) const;

private:
    std::uint8_t *source(std::uint32_t x, std::uint32_t y) const;
    std::uint8_t *destination(std::uint32_t x, std::uint32_t y) const;

    MockHost &host_;
};

}  // namespace av

#endif  // AV_PRESENT_SCENE_HPP
