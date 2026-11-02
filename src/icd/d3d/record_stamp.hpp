#ifndef AV_D3D_RECORD_STAMP_HPP
#define AV_D3D_RECORD_STAMP_HPP

#include <stdint.h>

namespace av::d3d {

class RecordStamp {
public:
    RecordStamp();
    RecordStamp(uint32_t tag, uint32_t id);

    bool     holds(uint32_t tag) const;
    uint32_t id() const;

private:
    uint32_t tag_;
    uint32_t id_;
};

}

#endif
