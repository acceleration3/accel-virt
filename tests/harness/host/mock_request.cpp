#include "harness/host/mock_request.hpp"

#include <cstdlib>
#include <cstring>

namespace av {

MockRequest::MockRequest(const std::uint8_t *bytes, std::size_t size)
    : bytes_(static_cast<std::uint8_t *>(std::malloc(size + 1u))), size_(size), response_{}, response_size_(0u),
      answered_(false)
{
    std::memcpy(bytes_, bytes, size);
}

MockRequest::~MockRequest() { std::free(bytes_); }

std::size_t MockRequest::read(std::size_t offset, std::uint8_t *buffer, std::size_t size) const
{
    if (offset >= size_) return 0u;
    const std::size_t count = size < size_ - offset ? size : size_ - offset;
    std::memcpy(buffer, bytes_ + offset, count);
    return count;
}

bool MockRequest::answer(const std::uint8_t *bytes, std::size_t size)
{
    if (answered_) return false;
    response_size_ = size < sizeof response_ ? size : sizeof response_;
    std::memcpy(response_, bytes, response_size_);
    answered_ = true;
    return true;
}

bool MockRequest::answered() const { return answered_; }

std::uint32_t MockRequest::response_type() const { return response_u32(0u); }

std::uint32_t MockRequest::response_u32(std::size_t offset) const
{
    std::uint32_t value = 0u;
    if (offset + sizeof value <= response_size_) std::memcpy(&value, response_ + offset, sizeof value);
    return value;
}

std::size_t MockRequest::response_size() const { return response_size_; }
const std::uint8_t *MockRequest::response_bytes() const { return response_; }

}  // namespace av
