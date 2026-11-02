#include "core/support/status.hpp"
#include "proto/virtio_gpu/page_request.hpp"
#include "proto/virtio_gpu/display_timing_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader  clock_reader(data, size);
    av::PageRequest clock;
    if (clock.decode(clock_reader) == av::Status::ok) {
        std::uint64_t address = 0u;
        std::memcpy(&address, data + 24u, sizeof address);
        if (clock.address() != address || address % av::PageRequest::page_size != 0u ||
            clock_reader.offset() != av::PageRequest::encoded_size)
            __builtin_trap();
    } else if (clock.address() != 0u) {
        __builtin_trap();
    }
    av::WireReader           timing_reader(data, size);
    av::DisplayTimingRequest timing;
    if (timing.decode(timing_reader) != av::Status::ok) {
        if (timing.head() != 0u || timing.refresh_millihz() != 0u) __builtin_trap();
        return 0;
    }
    std::uint32_t head = 0u;
    std::uint32_t refresh = 0u;
    std::memcpy(&head, data + 24u, sizeof head);
    std::memcpy(&refresh, data + 28u, sizeof refresh);
    if (timing.head() != head || timing.refresh_millihz() != refresh ||
        refresh < av::DisplayTimingRequest::least_millihz || refresh > av::DisplayTimingRequest::most_millihz ||
        timing_reader.offset() != av::DisplayTimingRequest::encoded_size)
        __builtin_trap();
    return 0;
}
