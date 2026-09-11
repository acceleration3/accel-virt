/* A hostile translated resource list. The parser may refuse anything; it must
 * never read past the bytes, and what it accepts must satisfy its invariants. */
#include "../../src/core/resource_list.hpp"
#include "../../src/core/status.hpp"
#include "../harness/mock_platform.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockPlatform platform;
    av::ResourceList list(platform);
    if (list.initialize(data, size) != av::Status::ok) return 0;
    if (!list.invariants_hold() || platform.unclaimed_violations() != 0u) __builtin_trap();
    std::uint64_t length = 0u;
    if (list.find_memory(0u, 0u, length) == av::Status::ok && length == 0u) __builtin_trap();
    return 0;
}
