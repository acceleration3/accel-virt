/* Drive the address space with an attacker-chosen sequence of operations and
 * addresses. No input may crash, leak, translate out of bounds, or break an
 * invariant. */
#include "../../src/core/gpu_virtual_address_space.hpp"
#include "../harness/mock_platform.hpp"

#include <cstddef>
#include <cstdint>

namespace {

class OperationStream {
public:
    OperationStream(const std::uint8_t *bytes, std::size_t length)
        : cursor_(bytes), end_(bytes + length) {}

    bool exhausted() const { return cursor_ >= end_; }

    std::uint8_t take_byte()
    {
        return (cursor_ < end_) ? *cursor_++ : 0u;
    }

    std::uint64_t take_quad()
    {
        std::uint64_t value = 0u;
        for (unsigned i = 0u; i < 8u; i++)
            value |= static_cast<std::uint64_t>(take_byte()) << (8u * i);
        return value;
    }

private:
    const std::uint8_t *cursor_;
    const std::uint8_t *end_;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockPlatform platform;
    av::GpuVirtualAddressSpace space(platform);
    if (space.initialize(av::ByteRange(0x1000000000ull, 0x400000ull)) != av::Status::ok)
        return 0;
    space.set_range_ceiling(64u);

    OperationStream stream(data, size);
    while (!stream.exhausted()) {
        const std::uint8_t opcode = stream.take_byte();
        switch (opcode & 0x7u) {
        case 0: {
            std::uint64_t reserved = 0u;
            const std::uint64_t size_request = stream.take_quad();
            const av::Alignment alignment(stream.take_quad());
            (void)space.reserve(size_request, alignment, reserved);
            break;
        }
        case 1: {
            const std::uint64_t base = stream.take_quad();
            const std::uint64_t span = stream.take_quad();
            const std::uint64_t offset = stream.take_quad();
            (void)space.map(base, span,
                            static_cast<std::uint32_t>(opcode >> 3) + 1u, offset);
            break;
        }
        case 2: (void)space.unmap(stream.take_quad()); break;
        case 3: (void)space.release(stream.take_quad()); break;
        case 4: {
            std::uint32_t object = 0u; std::uint64_t offset = 0u;
            if (space.translate(stream.take_quad(), object, offset) == av::Status::ok
                && object == 0u)
                __builtin_trap();   /* mapped implies a non-zero object */
            break;
        }
        default: {
            std::uint64_t reserved = 0u;
            (void)space.reserve(av::GpuVirtualAddressSpace::page_size,
                                av::Alignment(av::GpuVirtualAddressSpace::page_size),
                                reserved);
            break;
        }
        }
        if (!space.invariants_hold()) __builtin_trap();
    }
    /* A rule the address space broke is a finding, even when nothing crashed. */
    if (platform.unclaimed_violations() != 0u) __builtin_trap();
    return 0;
}
