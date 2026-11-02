#ifndef AV_WIRE_SPILL_HPP
#define AV_WIRE_SPILL_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireSpill {
public:
    WireSpill(const WireSpill &) = delete;
    WireSpill &operator=(const WireSpill &) = delete;

    virtual uint8_t *grow(const uint8_t *written, size_t used, size_t wanted, size_t &capacity) = 0;

protected:
    WireSpill() = default;
    ~WireSpill() = default;
};

}

#endif
