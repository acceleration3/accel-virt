#ifndef AV_PRESENT_REGION_HPP
#define AV_PRESENT_REGION_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class PresentRegion {
public:
    static constexpr size_t   encoded_size = 24u;
    static constexpr uint32_t pixel_bytes = 4u;

    PresentRegion();
    PresentRegion(uint32_t source_x, uint32_t source_y, uint32_t destination_x, uint32_t destination_y, uint32_t width,
                  uint32_t height);

    void                 write(WireWriter &writer) const;
    [[nodiscard]] Status decode(WireReader &reader);

    uint32_t source_x() const;
    uint32_t source_y() const;
    uint32_t destination_x() const;
    uint32_t destination_y() const;
    uint32_t width() const;
    uint32_t height() const;

private:
    uint32_t source_x_;
    uint32_t source_y_;
    uint32_t destination_x_;
    uint32_t destination_y_;
    uint32_t width_;
    uint32_t height_;
};

}

#endif
