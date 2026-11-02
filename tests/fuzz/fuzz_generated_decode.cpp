#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {
constexpr std::size_t arena_bytes = 2048u;

class DecodedPointer
{
public:
    DecodedPointer(const unsigned char *block, std::size_t bytes) : block_(block), bytes_(bytes) {}

    void must_be_ours_or_null(const void *pointer) const
    {
        if (pointer == nullptr) return;
        const auto *const byte = static_cast<const unsigned char *>(pointer);
        if (byte < block_ || byte >= block_ + bytes_) __builtin_trap();
    }

private:
    const unsigned char *block_;
    std::size_t bytes_;
};

template <typename T, typename Decode>
void decode_one(const std::uint8_t *data, std::size_t size, Decode decode)
{
    alignas(16) unsigned char block[arena_bytes];
    av::WireArena arena(block, sizeof block);
    av::WireReader reader(data, size);
    T decoded = {};
    const bool accepted = decode(reader, &decoded, arena);
    if (arena.used() > arena.capacity()) __builtin_trap();
    if (!accepted) return;
    if (reader.remaining() > size) __builtin_trap();
    const DecodedPointer ours(block, sizeof block);
    ours.must_be_ours_or_null(decoded.pNext);
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    {
        alignas(16) unsigned char block[arena_bytes];
        av::WireArena arena(block, sizeof block);
        av::WireReader reader(data, size);
        VkExtent2D extent = {};
        if (av::vkproto::deserialize_VkExtent2D(reader, &extent, arena) && arena.used() != 0u)
            __builtin_trap();
    }
    decode_one<VkApplicationInfo>(data, size, [](av::WireReader &r, VkApplicationInfo *out, av::WireArena &a) {
        const bool accepted = av::vkproto::deserialize_VkApplicationInfo(r, out, a);
        if (accepted && out->pApplicationName != nullptr && a.used() == 0u) __builtin_trap();
        return accepted;
    });
    decode_one<VkInstanceCreateInfo>(data, size, [](av::WireReader &r, VkInstanceCreateInfo *out, av::WireArena &a) {
        return av::vkproto::deserialize_VkInstanceCreateInfo(r, out, a);
    });
    return 0;
}
