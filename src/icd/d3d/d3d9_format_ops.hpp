/* The formats a Direct3D 9 adapter offers d3d9.dll (GetCaps GETFORMATCOUNT and
 * GETFORMATDATA): a FORMATOP per format, built from what the host's own
 * Direct3D 9 answered for it (QueryFormats9), never from a list written here -
 * a runtime told of a format the host cannot make makes it, and the device is
 * lost on the host instead. The formats asked are every one a Direct3D 9
 * application may check: the API's, the depth and block FourCCs games probe
 * (INTZ, DF16, DF24, ATI1, ATI2, NULL, RESZ), and the packed video ones. */
#ifndef AV_ICD_D3D9_FORMAT_OPS_HPP
#define AV_ICD_D3D9_FORMAT_OPS_HPP

#include <stdint.h>

namespace av::d3d {

class D3d9FormatOps {
public:
    /* What one offered format can be: FORMATOP_* operations and, as the
     * runtime reads FlipMsTypes and BltMsTypes, its multisample types, bit
     * (type - 1). */
    struct Op {
        uint32_t format_;
        uint32_t operations_;
        uint32_t multisample_;
    };

    static constexpr uint32_t max_formats = 80u;

    D3d9FormatOps();

    /* The formats to ask the host about, in order. */
    uint32_t formats_to_ask() const;
    uint32_t format_to_ask(uint32_t index) const;
    /* QueryFormats9's two words for `format`: offered when it can be anything
     * at all, withdrawn when it now cannot. A format not asked is ignored. */
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

}  // namespace av::d3d

#endif  // AV_ICD_D3D9_FORMAT_OPS_HPP
