/* What QEMU's base virtio-gpu device answers for the requests virtio-gpu-rvk's
 * device core leaves to it (virtio 1.2 5.7.6; virtio_gpu_simple_process_cmd):
 * display info for its one enabled scanout, success for the 2D resource and
 * scanout requests, and ERR_UNSPEC for a request too short to have a header
 * or of a type it does not know. Resources are not kept: the rig runs no
 * display, so nothing reads what a 2D resource would hold. */
#ifndef AV_BASE_VIRTIO_GPU_HPP
#define AV_BASE_VIRTIO_GPU_HPP

#include "core/display/display_info_response.hpp"

#include <cstdint>

namespace av {

class BaseVirtioGpu {
public:
    static constexpr std::uint32_t width = 1280u;
    static constexpr std::uint32_t height = 800u;

    BaseVirtioGpu();

    /* The answer to the `length` bytes of `request`, in at most `room` bytes of
     * `answer`: how many were written. */
    std::uint32_t answer(const std::uint8_t *request, std::uint32_t length, std::uint8_t *answer,
                         std::uint32_t room);
    /* Requests answered so far, of any type. */
    std::uint32_t answered() const;

private:
    static constexpr std::uint32_t header_bytes = 24u;
    static constexpr std::uint32_t flag_fence = 1u;

    /* The header of a response of `type` to `request`, carrying its fence. */
    void write_header(const std::uint8_t *request, std::uint32_t type, std::uint8_t *answer) const;

    std::uint8_t  display_info_[DisplayInfoResponse::encoded_size];
    std::uint32_t answered_;
};

}  // namespace av

#endif  // AV_BASE_VIRTIO_GPU_HPP
