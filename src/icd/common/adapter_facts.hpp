#ifndef AV_ICD_ADAPTER_FACTS_HPP
#define AV_ICD_ADAPTER_FACTS_HPP

#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "icd/common/adapter_string.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class TextBuffer;
}

namespace av::icd {

class AdapterFacts {
public:
    static constexpr uint32_t luid_fact = 1u << 0u;
    static constexpr uint32_t identity_fact = 1u << 1u;
    static constexpr uint32_t location_fact = 1u << 2u;
    static constexpr uint32_t name_fact = 1u << 3u;
    static constexpr uint32_t driver_version_fact = 1u << 4u;
    static constexpr uint32_t segments_fact = 1u << 5u;
    static constexpr uint32_t features_fact = 1u << 6u;

    AdapterFacts();

    void learn(Kmt &kmt, uint32_t adapter);
    void forget();
    bool knows(uint32_t facts) const;

    uint64_t           luid() const;
    const PciIdentity &identity() const;
    const PciLocation &location() const;
    const char        *name() const;
    uint64_t           driver_version() const;
    void               append_driver_version(TextBuffer &text) const;
    uint64_t           dedicated_video_bytes() const;
    uint64_t           dedicated_system_bytes() const;
    uint64_t           shared_system_bytes() const;
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

}

#endif
