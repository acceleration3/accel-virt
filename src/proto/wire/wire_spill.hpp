/* Where a WireWriter goes on writing once the room it was lent runs out.
 *
 * A writer is lent fixed room - a transport's staging, a batch's free space,
 * a reply block - and the length of what will be written is not known until
 * it has been: a program's source, a wait list, a build log. A writer lent
 * with a spill moves what it wrote here when the room runs out and carries
 * on, so the length is the caller's and not the room's (docs/protocol.md,
 * "Long commands"). A writer with none latches too_large, as it always has. */
#ifndef AV_WIRE_SPILL_HPP
#define AV_WIRE_SPILL_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireSpill {
public:
    WireSpill(const WireSpill &) = delete;
    WireSpill &operator=(const WireSpill &) = delete;

    /* Room for at least `wanted` bytes whose first `used` are a copy of
     * `written`, and in `capacity` how much room it is; null, with the
     * writer then latching too_large, when there is no memory for it.
     * `written` may be the spill's own room from an earlier grow, which it
     * then gives back. */
    virtual uint8_t *grow(const uint8_t *written, size_t used, size_t wanted, size_t &capacity) = 0;

protected:
    WireSpill() = default;
    /* Not virtual: nothing is destroyed through this pointer, and the kernel
     * has no operator delete for a deleting destructor (CLAUDE.md). */
    ~WireSpill() = default;
};

}  // namespace av

#endif  // AV_WIRE_SPILL_HPP
