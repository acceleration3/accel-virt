/* What Windows says of one avgpu adapter: its LUID, the PCI identity it
 * presents and where it sits, the name its driver gave it, the driver
 * package's version, its memory segments and whether its GPU is integrated -
 * each asked of the kernel once, and kept with whether it was given. Every
 * user-mode driver names and sizes an adapter from these (the Vulkan ICD's
 * physical devices, OpenCL's devices, the vendor libraries' GPUs), so that
 * every API describes one GPU alike. What to do without a fact is each
 * driver's own: the Vulkan ICD keeps the host's name, OpenCL leaves its
 * device as it was, the vendor libraries leave the adapter out. */
#ifndef AV_ICD_ADAPTER_FACTS_HPP
#define AV_ICD_ADAPTER_FACTS_HPP

#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "icd/common/adapter_string.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class TextBuffer;
}  // namespace av

namespace av::icd {

class AdapterFacts {
public:
    /* The facts, one bit each, as knows() takes them. */
    static constexpr uint32_t luid_fact = 1u << 0u;
    static constexpr uint32_t identity_fact = 1u << 1u;
    static constexpr uint32_t location_fact = 1u << 2u;
    static constexpr uint32_t name_fact = 1u << 3u;
    static constexpr uint32_t driver_version_fact = 1u << 4u;
    static constexpr uint32_t segments_fact = 1u << 5u;
    static constexpr uint32_t features_fact = 1u << 6u;

    AdapterFacts();

    /* Every fact of `adapter`, a kernel adapter handle: what Windows refuses
     * is unknown and reads as zero, or no name. What was known before is
     * forgotten first. */
    void learn(Kmt &kmt, uint32_t adapter);
    /* Nothing is known. */
    void forget();
    /* Whether every fact in `facts` was given. */
    bool knows(uint32_t facts) const;

    uint64_t           luid() const;
    const PciIdentity &identity() const;
    const PciLocation &location() const;
    /* UTF-8, ended; empty when unknown. */
    const char        *name() const;
    /* Four 16-bit parts, the most significant first. */
    uint64_t           driver_version() const;
    /* The driver package's version as Windows writes it, "0.35.5.0". */
    void               append_driver_version(TextBuffer &text) const;
    uint64_t           dedicated_video_bytes() const;
    uint64_t           dedicated_system_bytes() const;
    uint64_t           shared_system_bytes() const;
    /* The host GPU doing the work shares the host CPU's memory. A miniport
     * from before the bit says nothing of the GPU's kind, which reads as
     * discrete: what the adapter presented before it existed. */
    bool               integrated() const;

    bool invariants_hold() const;

private:
    uint32_t      known_;
    uint64_t      luid_;
    PciIdentity   identity_;
    PciLocation   location_;
    AdapterString name_;
    uint64_t      driver_version_;
    uint64_t      dedicated_video_;
    uint64_t      dedicated_system_;
    uint64_t      shared_system_;
    bool          integrated_;
};

}  // namespace av::icd

#endif  // AV_ICD_ADAPTER_FACTS_HPP
