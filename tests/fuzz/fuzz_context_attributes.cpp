#include "icd/opengl/context_attributes.hpp"
#include "proto/context/gl_context_options.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t values = 2u * av::glicd::ContextAttributes::max_pairs + 1u;

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    std::int32_t list[values] = {};
    std::memcpy(list, data, size < sizeof list ? size : sizeof list);
    av::glicd::ContextAttributes attributes(list);
    const std::uint32_t error = attributes.decode();
    require(attributes.invariants_hold());
    const av::GlContextOptions options = attributes.options();
    require(options.valid());
    require(options.share_context() == 0u);
    require(error == 0u || options.defaults());
    require(error == 0u || error == av::glicd::ContextAttributes::error_invalid_parameter ||
            error == av::glicd::ContextAttributes::error_invalid_version ||
            error == av::glicd::ContextAttributes::error_invalid_profile);
    return 0;
}
