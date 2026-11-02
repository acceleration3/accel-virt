#ifndef AV_GPU_REQUEST_BUILDER_HPP
#define AV_GPU_REQUEST_BUILDER_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class GpuRequestBuilder {
public:
    static constexpr std::size_t capacity = 56u + 16u * 16u;

    explicit GpuRequestBuilder(std::uint32_t type);

    GpuRequestBuilder &u32(std::uint32_t value);
    GpuRequestBuilder &u64(std::uint64_t value);
    GpuRequestBuilder &append(const std::uint8_t *bytes, std::size_t size);
    GpuRequestBuilder &zeros(std::size_t size);
    GpuRequestBuilder &context(std::uint32_t id);
    GpuRequestBuilder &truncate(std::size_t size);

    const std::uint8_t *bytes() const;
    std::size_t         size() const;

private:
    std::uint8_t bytes_[capacity];
    std::size_t  size_;
};

}

#endif
