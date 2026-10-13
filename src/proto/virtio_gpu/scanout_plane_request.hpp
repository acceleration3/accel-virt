/* SCANOUT_PLANE (docs/protocol.md, "Scanout planes"): one overlay plane of a
 * head, as the guest's last flip left it - the image the host shares under
 * `key`, its source rectangle shown in a destination one on the head, both
 * with the clip applied, and layered by `layer`, 0 the bottom. A plane that is
 * not shown is turned off. The head's size rides along, so the device composes
 * at it whatever display the head has. The header, then the head, the layer,
 * the flags, the key, the two rectangles left, top, right, bottom, and the
 * head's width and height. Which heads exist is the device's to judge. */
#ifndef AV_SCANOUT_PLANE_REQUEST_HPP
#define AV_SCANOUT_PLANE_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class ScanoutPlaneRequest {
public:
    static constexpr uint32_t type = 0x02F8u;
    static constexpr size_t   encoded_size = 88u;
    /* The layers a head has, avgpu's OverlayPlane::max_planes. */
    static constexpr uint32_t layers = 4u;
    static constexpr uint32_t flag_shown = 0x1u;
    static constexpr uint32_t flag_filtered = 0x2u;
    /* A YCbCr image, converted by the BT.709 matrix rather than BT.601's, from
     * full-range values rather than studio ones. */
    static constexpr uint32_t flag_ycbcr = 0x4u;
    static constexpr uint32_t flag_bt709 = 0x8u;
    static constexpr uint32_t flag_full_range = 0x10u;
    /* The largest head the device composes for, either way. */
    static constexpr uint32_t most_extent = 16384u;

    ScanoutPlaneRequest();
    /* A shown plane. `source` and `destination` are left, top, right, bottom. */
    ScanoutPlaneRequest(uint32_t head, uint32_t layer, uint32_t flags, uint64_t key, const int32_t *source,
                        const int32_t *destination, uint32_t head_width, uint32_t head_height);

    [[nodiscard]] Status encode(WireWriter &writer) const;
    /* malformed for a request too short, a layer past the last, a head out of
     * range, or a shown plane with an empty, negative or inverted rectangle or
     * no key. */
    [[nodiscard]] Status decode(WireReader &reader);
    uint32_t             head() const;
    uint32_t             layer() const;
    bool                 shown() const;
    bool                 filtered() const;
    bool                 ycbcr() const;
    bool                 bt709() const;
    bool                 full_range() const;
    uint64_t             key() const;
    const int32_t       *source() const;
    const int32_t       *destination() const;
    uint32_t             head_width() const;
    uint32_t             head_height() const;

private:
    static constexpr size_t header_size = 24u;

    uint32_t head_;
    uint32_t layer_;
    uint32_t flags_;
    uint64_t key_;
    int32_t  source_[4];
    int32_t  destination_[4];
    uint32_t head_width_;
    uint32_t head_height_;
};

}  // namespace av

#endif  // AV_SCANOUT_PLANE_REQUEST_HPP
