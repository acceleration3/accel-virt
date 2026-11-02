#ifndef AV_ICD_D3D9_FORMAT_OPS_HPP
#define AV_ICD_D3D9_FORMAT_OPS_HPP

#include <stdint.h>

namespace av::d3d {

class D3d9FormatOps {
public:
    struct Op {
        uint32_t format_;
        uint32_t operations_;
        uint32_t multisample_;
    };

    static constexpr uint32_t max_formats = 80u;

    D3d9FormatOps();

    uint32_t formats_to_ask() const;
    uint32_t format_to_ask(uint32_t index) const;
    void     record(uint32_t format, uint32_t support, uint32_t multisample);

    uint32_t  count() const;
    const Op &at(uint32_t index) const;

    bool invariants_hold() const;

private:
    uint32_t operations(uint32_t format, uint32_t support) const;
    bool     asked(uint32_t format) const;

    Op       offered_[max_formats];
    uint32_t count_;
};

}

#endif
