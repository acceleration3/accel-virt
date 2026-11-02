#include "device/abi/av_executor.h"
#include "executor/context_session.hpp"
#include "executor/vulkan/vulkan_backend.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);
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
constexpr std::size_t arena_bytes = 1u << 16;

alignas(16) std::uint8_t block[arena_bytes];

bool defined(std::uint32_t result)
{
    return result == AV_EXECUTED || result == AV_UNKNOWN || result == AV_UNDECODABLE ||
           result == AV_DEVICE_LOST;
}

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
    if (result == AV_EXECUTED && answer < capacity) {
        for (std::size_t i = answer; i < capacity; i++) {
            if (reply[i] != guard) __builtin_trap();
        }
    }
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::VulkanBackend  backend;
    av::ContextSession session;
    if (session.initialize(backend, block, sizeof block) != av::Status::ok) return 0;

    AnsweredHandles answered;
    std::size_t offset = 0u;
    for (std::size_t command = 0u; command < max_commands; command++) {
        if (size - offset < record_header_bytes) break;
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
    backend.shut_down();
    return 0;
}
