/* The private driver data a user-mode driver hands DdiPresent with a
 * blt-model present whose pixels it did not read back: the key of the present
 * snapshot it sent the host instead (the Direct3D command PresentSnapshot,
 * docs/protocol.md, "Present engine"), which the miniport queues to the
 * present engine as the present's source. */
#ifndef AV_PRESENT_SNAPSHOT_RECORD_HPP
#define AV_PRESENT_SNAPSHOT_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PresentSnapshotRecord {
public:
    static constexpr size_t encoded_size = 16u;

    PresentSnapshotRecord();
    explicit PresentSnapshotRecord(uint64_t key);

    uint64_t key() const;

    /* too_large for less room than the record. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for fewer bytes than the record, another magic, reserved bytes
     * that are not 0 or a key of 0. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x534B5652u;   /* "RVKS" */

    uint64_t key_;
};

}  // namespace av

#endif  // AV_PRESENT_SNAPSHOT_RECORD_HPP
