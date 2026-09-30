/* The keys a device's blt-model presents name their present snapshots under
 * (docs/protocol.md, "Present engine"): 64 random bits drawn once, then one
 * more per present, never 0. The host keeps every session's snapshots in one
 * place, so a key is what stops one process naming another's: it has to be
 * guessed, and the base is the processor's random numbers. */
#ifndef AV_PRESENT_SNAPSHOT_KEYS_HPP
#define AV_PRESENT_SNAPSHOT_KEYS_HPP

#include <stdint.h>

namespace av::d3d {

class PresentSnapshotKeys {
public:
    PresentSnapshotKeys();
    PresentSnapshotKeys(const PresentSnapshotKeys &) = delete;
    PresentSnapshotKeys &operator=(const PresentSnapshotKeys &) = delete;

    uint64_t next();

private:
    /* A processor's random-number instruction answers nothing now and then;
     * one that never does is a processor without it, and the time stamp
     * counter stands in. */
    static constexpr uint32_t attempts = 32u;

    uint64_t base_;
    uint64_t issued_;
};

}  // namespace av::d3d

#endif  // AV_PRESENT_SNAPSHOT_KEYS_HPP
