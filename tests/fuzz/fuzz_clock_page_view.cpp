/* The display clock's page as a hostile device writes it, read the way the
 * driver's DPC reads it: whatever a read accepts is a blank the page's bytes
 * say whole - a head the header describes, an even generation, a rate in
 * range with the period it has - and a refused read leaves the blank as it
 * was. Asking writes only the asking head's word. */
#include "miniport/display/clock_page_view.hpp"
#include "core/support/status.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/pages/clock_page_header.hpp"
#include "proto/pages/vblank_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockPlatform   platform;
    av::ClockPageView view(platform);
    if (view.initialize() != av::Status::ok) __builtin_trap();
    auto *const page = static_cast<std::uint8_t *>(
        platform.translate_physical(view.address(), av::ClockPageHeader::page_size));
    std::memcpy(page, data, size < av::ClockPageHeader::page_size ? size : av::ClockPageHeader::page_size);
    av::ClockPageHeader header;
    const bool          headed = header.decode(page, av::ClockPageHeader::encoded_size) == av::Status::ok;
    for (std::uint32_t head = 0u; head <= av::ClockPageHeader::max_heads; head++) {
        av::VblankRecord blank(1u, 2u, 3u, 60000u);
        const av::Status read = view.read(head, blank);
        if (read != av::Status::ok) {
            if (blank.sequence() != 2u || blank.latched() != 1u) __builtin_trap();
            continue;
        }
        if (!headed || head >= header.heads()) __builtin_trap();
        std::uint32_t generation = 0u;
        std::memcpy(&generation, page + header.record_offset(head), sizeof generation);
        if (generation % 2u != 0u) __builtin_trap();
        std::uint8_t again[av::VblankRecord::encoded_size] = {};
        if (blank.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        if (std::memcmp(again, page + header.record_offset(head) + av::VblankRecord::fields_at, sizeof again) != 0)
            __builtin_trap();
    }
    std::uint8_t before[av::ClockPageHeader::page_size];
    std::memcpy(before, page, sizeof before);
    view.ask(0u, 5u);
    for (std::size_t at = 0u; at < sizeof before; at++) {
        const bool asked = headed && at >= av::ClockPageHeader::encoded_size + av::VblankRecord::interrupt_every_at &&
                           at < av::ClockPageHeader::encoded_size + av::VblankRecord::interrupt_every_at + 4u;
        if (!asked && page[at] != before[at]) __builtin_trap();
    }
    return 0;
}
