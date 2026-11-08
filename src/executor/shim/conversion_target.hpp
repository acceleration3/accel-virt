#ifndef AV_CONVERSION_TARGET_HPP
#define AV_CONVERSION_TARGET_HPP

#include <stdint.h>

namespace av {

class ConversionTarget {
public:
    ConversionTarget(uint32_t source, uint32_t destination);

    bool     raw_copy() const;
    bool     convertible() const;
    bool     packed_bytes() const;
    bool     float_pass() const;
    bool     shader_encodes() const;
    uint32_t format() const;

private:
    uint32_t source_;
    uint32_t destination_;
};

}

#endif
