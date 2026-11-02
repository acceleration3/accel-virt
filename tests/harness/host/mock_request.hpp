#ifndef AV_MOCK_REQUEST_HPP
#define AV_MOCK_REQUEST_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class MockRequest {
public:
    MockRequest(const std::uint8_t *bytes, std::size_t size);
    ~MockRequest();
    MockRequest(const MockRequest &) = delete;
    MockRequest &operator=(const MockRequest &) = delete;

    std::size_t read(std::size_t offset, std::uint8_t *buffer, std::size_t size) const;
    bool        answer(const std::uint8_t *bytes, std::size_t size);

    bool          answered() const;
    std::uint32_t response_type() const;
    std::uint32_t response_u32(std::size_t offset) const;
    std::size_t   response_size() const;
    const std::uint8_t *response_bytes() const;

private:
    std::uint8_t *bytes_;
    std::size_t   size_;
    std::uint8_t  response_[256];
    std::size_t   response_size_;
    bool          answered_;
};

}

#endif
