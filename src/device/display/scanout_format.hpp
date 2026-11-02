#ifndef AV_SCANOUT_FORMAT_HPP
#define AV_SCANOUT_FORMAT_HPP

#include <stdint.h>

namespace av {

class ScanoutFormat {
public:
    static constexpr uint32_t b8g8r8a8 = 1u;
    static constexpr uint32_t b8g8r8x8 = 2u;
    static constexpr uint32_t a8r8g8b8 = 3u;
    static constexpr uint32_t x8r8g8b8 = 4u;
    static constexpr uint32_t r8g8b8a8 = 67u;
    static constexpr uint32_t x8b8g8r8 = 68u;
    static constexpr uint32_t a8b8g8r8 = 121u;
    static constexpr uint32_t r8g8b8x8 = 134u;

    explicit ScanoutFormat(uint32_t format);

    bool     scannable() const;
    uint32_t fourcc() const;
    uint32_t pixel_bytes() const;
    uint32_t format() const;

private:
    uint32_t format_;
};

}

#endif
