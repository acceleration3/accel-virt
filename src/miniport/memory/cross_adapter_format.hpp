#ifndef AV_CROSS_ADAPTER_FORMAT_HPP
#define AV_CROSS_ADAPTER_FORMAT_HPP

#include <stdint.h>

namespace av {

class CrossAdapterFormat {
public:
    explicit CrossAdapterFormat(uint32_t format);

    uint32_t bytes_per_pixel() const;

private:
    enum : uint32_t {
        a2b10g10r10 = 31u,
        a2r10g10b10 = 35u,
        a16b16g16r16f = 113u,
        a32b32g32r32f = 116u
    };

    uint32_t format_;
};

}

#endif
