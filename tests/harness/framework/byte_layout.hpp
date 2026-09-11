/* Reads a little-endian integer at an absolute offset, so layout tests state
 * each field's documented offset instead of re-decoding it. */
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

}  // namespace av

#endif  // AV_BYTE_LAYOUT_HPP
