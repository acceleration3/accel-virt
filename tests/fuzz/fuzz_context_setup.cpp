#include "core/support/status.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/sub_api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const av::ContextSetup untouched(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, 2u, 12u, 3u, 0u, 4u);
    av::ContextSetup       setup = untouched;
    if (setup.decode(data, size) != av::Status::ok) {
        std::uint8_t kept[av::ContextSetup::encoded_size] = {};
        std::uint8_t before[av::ContextSetup::encoded_size] = {};
        if (setup.encode(kept, sizeof kept) != av::Status::ok || untouched.encode(before, sizeof before) != av::Status::ok ||
            std::memcmp(kept, before, sizeof kept) != 0)
            __builtin_trap();
        return 0;
    }
    if (size != av::ContextSetup::encoded_size) __builtin_trap();
    std::uint8_t again[av::ContextSetup::encoded_size] = {};
    if (setup.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, size) != 0) __builtin_trap();
    if (!setup.options_fit()) __builtin_trap();
    if (setup.ring_resource() == 0u || setup.fence_resource() == 0u || setup.ring_resource() == setup.fence_resource())
        __builtin_trap();
    if (setup.ring_order() < 12u || setup.ring_order() > 24u) __builtin_trap();
    if (setup.sub_api() == av::SubApi::opencl && !setup.options().shares_only()) __builtin_trap();
    if (setup.sub_api() != av::SubApi::opengl && setup.sub_api() != av::SubApi::opencl && !setup.options().defaults())
        __builtin_trap();
    return 0;
}
