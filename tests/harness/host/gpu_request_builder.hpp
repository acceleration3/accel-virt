/* virtio-gpu control requests laid out byte for byte (virtio 1.2 5.7.6), for
 * tests that hand the device core the requests a guest sends. */
#ifndef AV_GPU_REQUEST_BUILDER_HPP
#define AV_GPU_REQUEST_BUILDER_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class GpuRequestBuilder {
public:
    static constexpr std::size_t capacity = 56u + 16u * 16u;

    /* A 24-byte header of `type`. */
    explicit GpuRequestBuilder(std::uint32_t type);

    GpuRequestBuilder &u32(std::uint32_t value);
    GpuRequestBuilder &u64(std::uint64_t value);
    GpuRequestBuilder &append(const std::uint8_t *bytes, std::size_t size);
    GpuRequestBuilder &zeros(std::size_t size);
    /* The header's context id. */
    GpuRequestBuilder &context(std::uint32_t id);
    /* Cuts the request to its first `size` bytes. */
    GpuRequestBuilder &truncate(std::size_t size);

    const std::uint8_t *bytes() const;
    std::size_t         size() const;

private:
    std::uint8_t bytes_[capacity];
    std::size_t  size_;
};

}  // namespace av

#endif  // AV_GPU_REQUEST_BUILDER_HPP
