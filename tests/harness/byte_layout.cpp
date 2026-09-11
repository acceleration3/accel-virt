#include "byte_layout.hpp"

#include "test_registry.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

ByteLayout::ByteLayout(const std::uint8_t *bytes, std::size_t length)
    : bytes_(bytes), length_(length) {}

std::uint64_t ByteLayout::at(std::size_t offset, std::size_t width) const
{
    if (offset + width > length_) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                "field at %zu+%zu past %zu bytes", offset, width, length_);
        return 0u;
    }
    std::uint64_t value = 0u;
    for (std::size_t i = 0u; i < width; i++)
        value |= static_cast<std::uint64_t>(bytes_[offset + i]) << (8u * i);
    return value;
}

}  // namespace av
