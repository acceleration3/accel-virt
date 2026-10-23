/* The escape a user-mode driver names the point of its next present of a host
 * surface with, where the present itself cannot carry it: d3d9.dll's
 * PresentCB builds the kernel's present and drops the private driver data a
 * PresentPointRecord rides in. The miniport keeps the point on the device's
 * own open of the allocation minted as `surface` - an open of another device
 * is not found - and the next blt from that allocation takes it, as it would
 * the present's own record (docs/shared-surfaces.md, "Points"). */
#ifndef AV_NEXT_POINT_RECORD_HPP
#define AV_NEXT_POINT_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class NextPointRecord {
public:
    static constexpr size_t   encoded_size = 16u;
    static constexpr uint32_t magic = 0x004E5641u;   /* "AVN" */
    static constexpr uint32_t withdraw_magic = 0x004A5641u;   /* "AVJ": "AVW" is HostWindowRecord's */
    static constexpr uint32_t pages_magic = 0x00555641u;   /* "AVU": "AVP" is the present records' */

    /* What the record says of the point: name it for the next present, take
     * back one named for a present that never reached the kernel, or say the
     * next present reads the allocation's pages (the host staged nothing). */
    enum class Action : uint8_t { name, withdraw, pages };

    NextPointRecord();
    NextPointRecord(uint64_t surface, uint32_t point, Action action = Action::name);

    uint64_t surface() const;
    uint32_t point() const;
    Action   action() const;

    /* too_large for less room than the record. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for a length other than the record's, another magic, or a
     * surface or point of 0. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    uint64_t surface_;
    uint32_t point_;
    Action   action_;
};

}  // namespace av

#endif  // AV_NEXT_POINT_RECORD_HPP
