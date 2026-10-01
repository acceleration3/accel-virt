/* The private driver data a user-mode driver hands DdiPresent with a present
 * of a host surface: the point on that surface the present published - its
 * nth present of it (docs/shared-surfaces.md, "Points"). The miniport pairs it
 * with the source allocation's own shared id, never a name the driver chose,
 * and queues the pair to the present engine as a blt's source, which the host
 * staged at that point. */
#ifndef AV_PRESENT_POINT_RECORD_HPP
#define AV_PRESENT_POINT_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PresentPointRecord {
public:
    static constexpr size_t encoded_size = 16u;

    PresentPointRecord();
    explicit PresentPointRecord(uint32_t point);

    uint32_t point() const;

    /* too_large for less room than the record. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for fewer bytes than the record, another magic, reserved bytes
     * that are not 0 or a point of 0. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00505641u;   /* "AVP" */

    uint32_t point_;
};

}  // namespace av

#endif  // AV_PRESENT_POINT_RECORD_HPP
