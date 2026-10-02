/* US1 of feature 039: does memory the host GPU exports reach the guest through
 * the hostmem region? The probe allocates a megabyte on the bound GPU, places
 * it in the region, writes a pattern through the device's own view, and reads
 * the region back - what the guest would read there. It runs once, at realize,
 * and leaves the region and the GPU as it found them. */
#ifndef AV_HOST_MEMORY_PROBE_HPP
#define AV_HOST_MEMORY_PROBE_HPP

#include "device/feasibility/probe_result.hpp"

#include <stdint.h>

namespace av {

class Host;

class HostMemoryProbe {
public:
    /* The mechanism is what is in question, not the capacity: a mapping that
     * carries a megabyte carries a gigabyte, or fails with a number that says
     * why. The host tool sweeps the sizes. */
    static constexpr uint64_t probe_bytes = 0x100000u;

    explicit HostMemoryProbe(Host &host);
    HostMemoryProbe(const HostMemoryProbe &) = delete;
    HostMemoryProbe &operator=(const HostMemoryProbe &) = delete;

    /* Main loop. A second call changes nothing. */
    void               run();
    const ProbeResult &result() const;

private:
    static constexpr uint64_t sample_bytes = 64u;

    /* The byte a mapping that carries this memory must show at `offset`. */
    uint8_t      expected(uint64_t offset) const;
    /* ok when the region shows the pattern at `offset`. */
    [[nodiscard]] Status matches(uint64_t offset);

    Host       &host_;
    ProbeResult result_;
};

}  // namespace av

#endif  // AV_HOST_MEMORY_PROBE_HPP
