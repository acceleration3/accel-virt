#ifndef AV_BASE_VIRTIO_GPU_HPP
#define AV_BASE_VIRTIO_GPU_HPP

#include "miniport/display/display_info_response.hpp"

#include <cstdint>

namespace av {

class BaseVirtioGpu {
public:
    static constexpr std::uint32_t width = 1280u;
    static constexpr std::uint32_t height = 800u;

    BaseVirtioGpu();

    std::uint32_t answer(const std::uint8_t *request, std::uint32_t length, std::uint8_t *answer,
                         std::uint32_t room);
    std::uint32_t answered() const;

private:
    static constexpr std::uint32_t header_bytes = 24u;
    static constexpr std::uint32_t flag_fence = 1u;

    void write_header(const std::uint8_t *request, std::uint32_t type, std::uint8_t *answer) const;

    std::uint8_t  display_info_[DisplayInfoResponse::encoded_size];
    std::uint32_t answered_;
};

}

#endif
