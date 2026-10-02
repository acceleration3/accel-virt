/* US2 of feature 039: can the host GPU address guest pages directly? The probe
 * maps a megabyte of guest RAM, asks the host to import that window, writes a
 * pattern into it, and compares what the GPU sees through the import. A host
 * that cannot import is a clean refusal - the copy fallback is what the guest
 * then uses - but a GPU that sees something else is a lie worth a bit's
 * absence.
 *
 * The window is guest memory, so the probe keeps a copy of it and puts it back:
 * it needs that copy anyway, because copying it is exactly what the fallback
 * would have to do, and timing it is the number the refusal is worth. Without
 * somewhere to keep it the probe refuses rather than write over guest memory it
 * cannot restore. */
#ifndef AV_GUEST_IMPORT_PROBE_HPP
#define AV_GUEST_IMPORT_PROBE_HPP

#include "device/feasibility/probe_result.hpp"

#include <stdint.h>

namespace av {

class Host;

class GuestImportProbe {
public:
    static constexpr uint64_t probe_bytes = 0x100000u;

    explicit GuestImportProbe(Host &host);
    GuestImportProbe(const GuestImportProbe &) = delete;
    GuestImportProbe &operator=(const GuestImportProbe &) = delete;

    /* Main loop. `guest_address` is where the probe's window starts; a second
     * call changes nothing. */
    void               run(uint64_t guest_address);
    const ProbeResult &result() const;

private:
    static constexpr uint64_t sample_bytes = 64u;
    static constexpr uint64_t nanoseconds_per_second = 1000000000u;

    uint8_t              expected(uint64_t offset) const;
    [[nodiscard]] Status matches(void *imported, uint64_t offset);
    /* `probe_bytes` from `from` to `to`, and how many bytes a second that was;
     * 0 when the clock could not see it. */
    uint64_t             copy(uint8_t *to, const uint8_t *from);

    Host       &host_;
    ProbeResult result_;
};

}  // namespace av

#endif  // AV_GUEST_IMPORT_PROBE_HPP
