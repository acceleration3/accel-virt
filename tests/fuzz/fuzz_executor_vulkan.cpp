/* The ABI boundary above the generated decoder.
 *
 * Feature 063 fuzzes the decoder itself; this fuzzes what a guest can actually
 * reach through the executor: command ids it chose, payloads it wrote, and
 * reply blocks it sized. The required outcome of each is one of the four
 * results the ABI defines - never a crash, and never a write past the reply
 * capacity.
 *
 * An input is a batch, run one command after another in one context, because
 * the holes that matter take more than one: a buffer destroyed and then named
 * again, an instance destroyed and its physical device used (T031). Each record
 * is a two-byte command within Vulkan's range, a two-byte length, a byte
 * choosing how much room the answer gets, and that many payload bytes. The
 * handles the backend answers live at addresses no corpus can know, so eight
 * payload bytes reading 0xC1C1C1C1 in their top half stand for the handle whose
 * index is in their bottom half, in the order the replies first carried them -
 * and it goes on standing for it after the guest destroyed it, which is the
 * point. */
#include "device/abi/av_executor.h"
#include "executor/context_session.hpp"
#include "executor/decode_arena.hpp"
#include "executor/vulkan/vulkan_backend.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);
/* This target makes real VkInstances through the decoder, and the drivers
 * behind them allocate from modules that carry no symbols, so a suppression by
 * name cannot name them. Leak detection is therefore off here and on in
 * tests/unit/executor/vulkan/test_vulkan_backend.cpp, where the same objects
 * are made and the drivers involved can be named. It did its work first: the
 * leak it found - a backend shared by every session, so a context that died
 * owning a VkInstance left it behind - is fixed, and the input that showed it
 * is in this target's corpus. */
extern "C" int __lsan_is_turned_off(void);
extern "C" int __lsan_is_turned_off(void) { return 1; }

namespace {

constexpr std::size_t   record_header_bytes = 5u;
constexpr std::size_t   max_payload_bytes = 1024u;
constexpr std::size_t   max_commands = 16u;
constexpr std::size_t   reply_bytes = 256u;
constexpr std::size_t   max_answered = 64u;
constexpr std::uint32_t vulkan_api = 1u;
constexpr std::uint32_t answered_marker = 0xC1C1C1C1u;
constexpr std::uint8_t  guard = 0xABu;
/* Smaller than the library's own block, because what is being looked for is
 * the refusal rather than the megabyte. */
constexpr std::size_t arena_bytes = 1u << 16;

alignas(16) std::uint8_t block[arena_bytes];

bool defined(std::uint32_t result)
{
    return result == AV_EXECUTED || result == AV_UNKNOWN || result == AV_UNDECODABLE ||
           result == AV_DEVICE_LOST;
}

/* Every handle a reply carried, first time seen. Read at every byte offset
 * rather than by each command's layout, and kept only when the backend knows
 * the number - which a stray eight bytes of a reply practically never are. */
class AnsweredHandles {
public:
    void note(const av::VulkanBackend &backend, const std::uint8_t *reply, std::size_t length)
    {
        for (std::size_t at = 0u; at + sizeof(std::uint64_t) <= length; at++) {
            std::uint64_t value = 0u;
            std::memcpy(&value, reply + at, sizeof value);
            if (value == 0u || !backend.knows_handle(value) || seen(value)) continue;
            if (count_ == max_answered) return;
            handles_[count_++] = value;
        }
    }

    /* `payload` with every marked eight bytes replaced by an answered handle;
     * an index past what was answered becomes zero, the API's null. */
    void substitute(std::uint8_t *payload, std::size_t length) const
    {
        for (std::size_t at = 0u; at + sizeof(std::uint64_t) <= length; at++) {
            std::uint64_t value = 0u;
            std::memcpy(&value, payload + at, sizeof value);
            if (value >> 32u != answered_marker) continue;
            const std::uint32_t index = static_cast<std::uint32_t>(value);
            value = index < count_ ? handles_[index] : 0u;
            std::memcpy(payload + at, &value, sizeof value);
            at += sizeof value - 1u;
        }
    }

private:
    bool seen(std::uint64_t value) const
    {
        for (std::size_t i = 0u; i < count_; i++) {
            if (handles_[i] == value) return true;
        }
        return false;
    }

    std::uint64_t handles_[max_answered] = {};
    std::size_t   count_ = 0u;
};

/* One command, judged: a defined result, nothing past the capacity, and no
 * length claimed shorter than what was written. */
void run_one(av::ContextSession &session, std::uint32_t command, const std::uint8_t *payload,
             std::size_t length, std::size_t capacity, std::uint8_t *reply, std::size_t &answer,
             std::uint32_t &result)
{
    std::memset(reply, guard, reply_bytes + 1u);
    result = session.execute(command, payload, length, capacity != 0u ? reply : nullptr, capacity,
                             answer);
    if (!defined(result)) __builtin_trap();
    for (std::size_t i = capacity; i < reply_bytes + 1u; i++) {
        if (reply[i] != guard) __builtin_trap();
    }
    /* An answer that does not fit may leave a prefix of itself in the block -
     * a streaming encoder writes fields until one does not fit - and the
     * device discards it. What may never happen is a length claimed shorter
     * than what was written. */
    if (result == AV_EXECUTED && answer < capacity) {
        for (std::size_t i = answer; i < capacity; i++) {
            if (reply[i] != guard) __builtin_trap();
        }
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::VulkanBackend  backend;
    av::ContextSession session;
    if (session.initialize(backend, block, sizeof block) != av::Status::ok) return 0;

    AnsweredHandles answered;
    std::size_t offset = 0u;
    for (std::size_t command = 0u; command < max_commands; command++) {
        if (size - offset < record_header_bytes) break;
        /* Steered into Vulkan's range, because the device filters by API
         * before an executor is ever called and a fuzzer spending its time
         * outside it would only ever see AV_UNKNOWN. */
        const std::uint32_t id = (vulkan_api << 16u) | data[offset] |
                                 (static_cast<std::uint32_t>(data[offset + 1u]) << 8u);
        std::size_t length = data[offset + 2u] | (static_cast<std::size_t>(data[offset + 3u]) << 8u);
        const std::size_t capacity = data[offset + 4u] % (reply_bytes + 1u);
        offset += record_header_bytes;
        if (length > size - offset) length = size - offset;
        if (length > max_payload_bytes) length = max_payload_bytes;

        std::uint8_t payload[max_payload_bytes];
        std::memcpy(payload, data + offset, length);
        offset += length;
        answered.substitute(payload, length);

        std::uint8_t  reply[reply_bytes + 1u];
        std::size_t   answer = 0u;
        std::uint32_t result = AV_UNKNOWN;
        run_one(session, id, payload, length, capacity, reply, answer, result);
        if (result == AV_EXECUTED && answer <= capacity) answered.note(backend, reply, answer);
    }
    session.release();
    /* What the guest never destroyed, which is what a dying context owes. */
    backend.shut_down();
    return 0;
}
