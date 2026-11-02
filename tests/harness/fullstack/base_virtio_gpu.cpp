#include "harness/fullstack/base_virtio_gpu.hpp"

#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstring>

namespace av {

namespace {

enum : std::uint32_t {
    get_display_info = 0x0100u,
    first_2d_request = 0x0101u,
    last_2d_request = 0x0107u,
};

constexpr std::uint32_t width_offset = 8u;
constexpr std::uint32_t height_offset = 12u;
constexpr std::uint32_t enabled_offset = 16u;

}

BaseVirtioGpu::BaseVirtioGpu() : display_info_{}, answered_(0u)
{
    const std::uint32_t type = DisplayInfoResponse::response_type;
    const std::uint32_t enabled = 1u;
    std::memcpy(display_info_, &type, sizeof type);
    std::memcpy(display_info_ + header_bytes + width_offset, &width, sizeof width);
    std::memcpy(display_info_ + header_bytes + height_offset, &height, sizeof height);
    std::memcpy(display_info_ + header_bytes + enabled_offset, &enabled, sizeof enabled);
}

void BaseVirtioGpu::write_header(const std::uint8_t *request, std::uint32_t type, std::uint8_t *answer) const
{
    constexpr std::uint32_t flags_offset = 4u;
    constexpr std::uint32_t fence_offset = 8u;
    std::uint32_t flags = 0u;
    std::memcpy(&flags, request + flags_offset, sizeof flags);
    flags &= flag_fence;
    std::memset(answer, 0, header_bytes);
    std::memcpy(answer, &type, sizeof type);
    std::memcpy(answer + flags_offset, &flags, sizeof flags);
    if (flags != 0u) std::memcpy(answer + fence_offset, request + fence_offset, sizeof(std::uint64_t));
}

std::uint32_t BaseVirtioGpu::answer(const std::uint8_t *request, std::uint32_t length, std::uint8_t *answer,
                                    std::uint32_t room)
{
    answered_++;
    if (room < header_bytes) return 0u;
    if (length < header_bytes) {
        const std::uint32_t refused = GpuResponse::err_unspec;
        std::memset(answer, 0, header_bytes);
        std::memcpy(answer, &refused, sizeof refused);
        return header_bytes;
    }
    std::uint32_t type = 0u;
    std::memcpy(&type, request, sizeof type);
    if (type == get_display_info) {
        const std::uint32_t written = room < sizeof display_info_ ? room : sizeof display_info_;
        write_header(request, DisplayInfoResponse::response_type, answer);
        std::memcpy(answer + header_bytes, display_info_ + header_bytes, written - header_bytes);
        return written;
    }
    const bool known = type >= first_2d_request && type <= last_2d_request;
    write_header(request, known ? GpuResponse::ok_nodata : GpuResponse::err_unspec, answer);
    return header_bytes;
}

std::uint32_t BaseVirtioGpu::answered() const { return answered_; }

}
