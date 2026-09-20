/* The adapter as DXGI and DXCore read it (contracts/umd_interface.md). Every
 * figure comes from the two D3DKMT queries the Vulkan ICD makes on the same
 * adapter - D3DKMT_ENUMADAPTERS2's LUID and D3DKMTQueryVideoMemoryInfo's
 * budgets - so the two drivers cannot report different numbers for one GPU.
 *
 * A budget the kernel refuses is reported as zero and logged once: a number
 * this driver does not know is not a reason to hide the adapter. The LUID and
 * the PCI identity are not numbers in that sense - they are what names the
 * adapter - so a refusal of either fails the learn. */
#ifndef AV_D3D_ADAPTER_IDENTITY_HPP
#define AV_D3D_ADAPTER_IDENTITY_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class StateWriter;
}  // namespace av

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av::d3d {

struct AdapterIdentityProbe;

class AdapterIdentity {
public:
    static constexpr uint32_t description_bytes = 64u;

    AdapterIdentity();
    AdapterIdentity(const AdapterIdentity &) = delete;
    AdapterIdentity &operator=(const AdapterIdentity &) = delete;

    /* not_supported when the LUID or the PCI identity cannot be had, each
     * logged once; ok otherwise, with a refused budget left at zero and
     * logged. Learning again overwrites rather than accumulates. */
    [[nodiscard]] Status learn(Kmt &kmt, uint32_t adapter, icd::LogSink &log);

    uint64_t    luid() const;
    uint32_t    vendor_id() const;
    uint32_t    device_id() const;
    uint64_t    dedicated_video_memory() const;
    uint64_t    shared_system_memory() const;
    const char *description() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    /* The group's budget, or zero with one log line. */
    uint64_t budget(Kmt &kmt, uint32_t adapter, icd::LogSink &log, uint32_t group);

    uint64_t luid_;
    uint32_t vendor_id_;
    uint32_t device_id_;
    uint64_t dedicated_;
    uint64_t shared_;
    char     description_[description_bytes];

    friend struct AdapterIdentityProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_ADAPTER_IDENTITY_HPP
