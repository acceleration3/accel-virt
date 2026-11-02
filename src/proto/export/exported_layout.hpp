#ifndef AV_EXPORTED_LAYOUT_HPP
#define AV_EXPORTED_LAYOUT_HPP

#include <stdint.h>

namespace av {

class ExportedLayout {
public:
    ExportedLayout();
    ExportedLayout(uint32_t width, uint32_t height, uint32_t fourcc, uint32_t stride, uint32_t offset,
                   uint64_t modifier);

    uint32_t width() const;
    uint32_t height() const;
    uint32_t fourcc() const;
    uint32_t stride() const;
    uint32_t offset() const;
    uint64_t modifier() const;
    bool     describes_a_picture() const;
    bool     empty() const;

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t fourcc_;
    uint32_t stride_;
    uint32_t offset_;
    uint64_t modifier_;
};

}

#endif
