/* The pixels a present moves (feature 046), over attacker-chosen layouts,
 * page counts, rectangles and sub-rectangles. Both surfaces are guest pages the
 * miniport did not write, and the rectangles come from a user-mode present, so
 * every one of them is hostile. Whatever is refused, nothing may be written
 * outside the destination's own pages, no page may be left mapped, and no IRQL
 * rule may break. */
#include "miniport/ddi/rect_record.hpp"
#include "miniport/present/mapped_pixels.hpp"
#include "miniport/present/present_blit.hpp"
#include "miniport/present/present_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/dma/rectangle.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

/* Two pages a surface may use, and a third the present must never reach. */
constexpr std::uint32_t pages = 2u;
constexpr std::uint32_t block_pages = pages + 1u;
constexpr std::uint64_t block_bytes = block_pages * av::GuestPageList::page_bytes;
constexpr std::uint32_t max_rects = 8u;
constexpr std::uint8_t  guard = 0xC3u;

av::Rectangle read_rectangle(av::WireReader &reader)
{
    const auto left = static_cast<std::int32_t>(reader.read_u32());
    const auto top = static_cast<std::int32_t>(reader.read_u32());
    const auto right = static_cast<std::int32_t>(reader.read_u32());
    const auto bottom = static_cast<std::int32_t>(reader.read_u32());
    return av::Rectangle(left, top, right, bottom);
}

av::SurfaceLayout read_layout(av::WireReader &reader)
{
    const std::uint32_t width = reader.read_u16();
    const std::uint32_t height = reader.read_u16();
    const std::uint32_t pitch = reader.read_u16();
    return av::SurfaceLayout(width, height, 21u, pitch);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockPlatform platform;
    auto *const      source_block = static_cast<std::uint8_t *>(platform.allocate_contiguous(block_bytes));
    auto *const      destination_block = static_cast<std::uint8_t *>(platform.allocate_contiguous(block_bytes));
    if (source_block == nullptr || destination_block == nullptr) return 0;
    std::memset(source_block, guard, block_bytes);
    std::memset(destination_block, guard, block_bytes);

    av::WireReader reader(data, size);
    std::uint64_t  source_frames[pages] = {};
    std::uint64_t  destination_frames[pages] = {};
    for (std::uint32_t index = 0u; index < pages; index++) {
        source_frames[index] = platform.physical_address(source_block) + index * av::GuestPageList::page_bytes;
        destination_frames[index] =
            platform.physical_address(destination_block) + index * av::GuestPageList::page_bytes;
    }
    const std::uint32_t      source_pages = reader.read_u8() % (pages + 1u);
    const std::uint32_t      destination_pages = reader.read_u8() % (pages + 1u);
    const av::SurfaceLayout  source_layout = read_layout(reader);
    const av::SurfaceLayout  destination_layout = read_layout(reader);
    const av::PresentSurface source(source_layout, av::GuestPageList(source_frames, source_pages));
    const av::PresentSurface destination(destination_layout,
                                         av::GuestPageList(destination_frames, destination_pages));
    const std::uint8_t       choices = reader.read_u8();
    const std::uint32_t      colour = reader.read_u32();
    const std::uint32_t      count = reader.read_u8() % (max_rects + 1u);
    const av::Rectangle      from = read_rectangle(reader);
    const av::Rectangle      into = read_rectangle(reader);
    av::RectRecord           rects[max_rects] = {};
    for (av::RectRecord &rect : rects) {
        const av::Rectangle sub = read_rectangle(reader);
        rect = av::RectRecord{sub.left(), sub.top(), sub.right(), sub.bottom()};
    }

    av::MappedPixels pixels(platform);
    av::PresentBlit  blit(platform, pixels, (choices & 1u) != 0u ? nullptr : &source, destination);
    const av::Status moved = (choices & 1u) != 0u ? blit.fill(colour, into, count != 0u ? rects : nullptr, count)
                                                 : blit.blit(from, into, count != 0u ? rects : nullptr, count);
    static_cast<void>(moved);
    if (!blit.invariants_hold()) __builtin_trap();
    /* The page past the ones the surface was given is nobody's, on either side. */
    for (std::uint64_t at = pages * av::GuestPageList::page_bytes; at < block_bytes; at++)
        if (destination_block[at] != guard || source_block[at] != guard) __builtin_trap();
    /* A present leaves nothing mapped, whatever it refused. */
    if (platform.mapped_frames() != 0u) __builtin_trap();
    platform.release_contiguous(source_block, block_bytes);
    platform.release_contiguous(destination_block, block_bytes);
    return 0;
}
