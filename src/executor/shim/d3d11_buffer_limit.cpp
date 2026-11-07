#include "executor/shim/d3d11_buffer_limit.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {
constexpr uint32_t mebibyte_shift = 20u;
constexpr uint32_t quarter_shift = 2u;
}

D3d11BufferLimit::D3d11BufferLimit(uint64_t video_memory_bytes) : video_memory_(video_memory_bytes)
{
    AV_SCOPE("D3d11BufferLimit::D3d11BufferLimit");
}

uint32_t D3d11BufferLimit::mebibytes() const
{
    AV_SCOPE("D3d11BufferLimit::mebibytes");
    const uint64_t quarter = (video_memory_ >> mebibyte_shift) >> quarter_shift;
    const uint64_t wanted = quarter > least_mebibytes ? quarter : least_mebibytes;
    return static_cast<uint32_t>(wanted < most_mebibytes ? wanted : most_mebibytes);
}

}
