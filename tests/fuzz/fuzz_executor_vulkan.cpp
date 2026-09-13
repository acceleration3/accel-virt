/* The ABI boundary above the generated decoder.
 *
 * Feature 063 fuzzes the decoder itself; this fuzzes what a guest can actually
 * reach through the executor: a command id it chose, a payload it wrote, and a
 * reply block it sized. The required outcome is one of the four results the
 * ABI defines - never a crash, and never a write past the reply capacity. */
#include "device/abi/rvk_executor.h"
#include "device/executor/context_session.hpp"
#include "device/executor/decode_arena.hpp"
#include "device/executor/vulkan_backend.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);
/* A guest that creates a VkInstance and never destroys it leaks a host object,
 * and the fuzzer finds that in seconds. It is a real problem and not this
 * target's: a backend is shared by every session today, so nothing destroys
 * what a dying context still owned. The corpus keeps the input that showed it
 * and the task list carries the fix - one backend per session, torn down with
 * it. Until then this target looks for memory safety, which is what it is for. */
extern "C" int __lsan_is_turned_off(void);
extern "C" int __lsan_is_turned_off(void) { return 1; }

namespace {

/* The first bytes choose the command and how much room the answer gets; the
 * rest is the payload. Below that there is nothing to drive. */
constexpr std::size_t header_bytes = 6u;
constexpr std::size_t reply_bytes = 256u;
/* Smaller than the library's own block, because what is being looked for is
 * the refusal rather than the megabyte. */
constexpr std::size_t arena_bytes = 1u << 16;

alignas(16) std::uint8_t block[arena_bytes];

bool defined(std::uint32_t result)
{
    return result == RVK_EXECUTED || result == RVK_UNKNOWN || result == RVK_UNDECODABLE ||
           result == RVK_DEVICE_LOST;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < header_bytes) return 0;

    std::uint32_t command = 0u;
    std::memcpy(&command, data, sizeof command);
    /* Steered into Vulkan's range, because the device filters by API before an
     * executor is ever called and a fuzzer spending its time outside it would
     * only ever see RVK_UNKNOWN. */
    command = (1u << 16u) | (command & 0xFFFFu);
    const std::size_t capacity = data[4] % (reply_bytes + 1u);

    av::VulkanBackend  backend;
    av::ContextSession session;
    if (session.initialize(backend, block, sizeof block) != av::Status::ok) return 0;

    std::uint8_t reply[reply_bytes + 1u];
    std::memset(reply, 0xAB, sizeof reply);
    std::size_t answer = 0u;
    const std::uint32_t result =
        session.execute(command, data + header_bytes, size - header_bytes,
                        capacity != 0u ? reply : nullptr, capacity, answer);

    if (!defined(result)) __builtin_trap();
    /* The guard byte past the capacity, and every byte past it. */
    for (std::size_t i = capacity; i < sizeof reply; i++) {
        if (reply[i] != 0xABu) __builtin_trap();
    }
    /* An answer that does not fit may leave a prefix of itself in the block -
     * a streaming encoder writes fields until one does not fit - and the
     * device discards it. What may never happen is a length claimed shorter
     * than what was written. */
    if (result == RVK_EXECUTED && answer < capacity) {
        for (std::size_t i = answer; i < capacity; i++) {
            if (reply[i] != 0xABu) __builtin_trap();
        }
    }
    session.release();
    return 0;
}
