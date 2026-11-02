#ifndef AV_BYTE_LAYOUT_HPP
#define AV_BYTE_LAYOUT_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class ByteLayout {
public:
    ByteLayout(const std::uint8_t *bytes, std::size_t length);

    std::uint64_t at(std::size_t offset, std::size_t width) const;

private:
    const std::uint8_t *bytes_;
    std::size_t         length_;
};

}

#endif
