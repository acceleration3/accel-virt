/* A hostile device driving one virtqueue's used ring, interleaved with the
 * driver adding and harvesting. Nothing the device writes may corrupt the
 * driver's shadow: invariants hold, free descriptors never exceed the size,
 * and a harvested head was always one the driver published. */
#include "core/support/status.hpp"
#include "miniport/virtio/virt_queue.hpp"
#include "miniport/virtio/virtq_segment.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockPlatform platform;
    av::VirtQueue queue(platform, 0u);
    if (queue.initialize(8u) != av::Status::ok) __builtin_trap();
    av::FakeVirtqueueDevice device(platform, queue.descriptor_area(), queue.driver_area(),
                                   queue.device_area(), queue.size());
    bool published[8] = {};
    av::WireReader reader(data, size);
    /* A read past the end latches and stops advancing, so stop on failure too. */
    while (reader.remaining() != 0u && reader.status() == av::Status::ok) {
        const std::uint8_t op = reader.read_u8();
        switch (op % 5u) {
        case 0: {
            const av::VirtqSegment segments[] = {
                av::VirtqSegment(0x1000u, reader.read_u16(), av::SegmentDirection::device_reads),
                av::VirtqSegment(0x2000u, reader.read_u16(), av::SegmentDirection::device_writes),
            };
            std::uint16_t head = 0u;
            if (queue.add(segments, static_cast<std::uint16_t>(1u + (op >> 7)), head) == av::Status::ok)
                published[head] = true;
            break;
        }
        case 1: (void)device.complete_all(reader.read_u16()); break;
        case 2: device.write_used(reader.read_u16(), reader.read_u32(), reader.read_u32()); break;
        case 3: device.set_used_index(reader.read_u16()); break;
        default: {
            std::uint16_t head = 0u; std::uint32_t written = 0u;
            if (queue.take_used(head, written) == av::Status::ok) {
                if (!published[head]) __builtin_trap();
                published[head] = false;
            }
            break;
        }
        }
        if (!queue.invariants_hold() || queue.free_descriptors() > queue.size()) __builtin_trap();
    }
    if (platform.unclaimed_violations() != 0u) __builtin_trap();
    return 0;
}
