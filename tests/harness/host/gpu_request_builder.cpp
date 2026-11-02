#include "harness/host/gpu_request_builder.hpp"

#include <cstring>

namespace av {

GpuRequestBuilder::GpuRequestBuilder(std::uint32_t type) : bytes_{}, size_(24u)
{
    std::memcpy(bytes_, &type, sizeof type);
}

GpuRequestBuilder &GpuRequestBuilder::u32(std::uint32_t value)
{
    std::memcpy(bytes_ + size_, &value, sizeof value);
    size_ += sizeof value;
    return *this;
}

GpuRequestBuilder &GpuRequestBuilder::u64(std::uint64_t value)
{
    std::memcpy(bytes_ + size_, &value, sizeof value);
    size_ += sizeof value;
    return *this;
}

GpuRequestBuilder &GpuRequestBuilder::append(const std::uint8_t *bytes, std::size_t size)
{
    std::memcpy(bytes_ + size_, bytes, size);
    size_ += size;
    return *this;
}

GpuRequestBuilder &GpuRequestBuilder::zeros(std::size_t size)
{
    std::memset(bytes_ + size_, 0, size);
    size_ += size;
    return *this;
}

GpuRequestBuilder &GpuRequestBuilder::context(std::uint32_t id)
{
    std::memcpy(bytes_ + 16u, &id, sizeof id);
    return *this;
}

GpuRequestBuilder &GpuRequestBuilder::truncate(std::size_t size)
{
    size_ = size;
    return *this;
}

const std::uint8_t *GpuRequestBuilder::bytes() const { return bytes_; }
std::size_t GpuRequestBuilder::size() const { return size_; }

}
