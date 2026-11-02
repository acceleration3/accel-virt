#ifndef AV_CLICD_CL_EXTENSION_POLICY_HPP
#define AV_CLICD_CL_EXTENSION_POLICY_HPP

#include "icd/common/extension_rows.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {
class ExtensionWords;
}

namespace av::clicd {

struct ClExtensionPolicyProbe;

class ClExtensionPolicy {
public:
    using Row = icd::ExtensionRow;

    static constexpr uint32_t max_rows = 32u;
    static constexpr char answered_here[] = "-";

    ClExtensionPolicy(const Row *rows, uint32_t count);
    ClExtensionPolicy(const ClExtensionPolicy &) = delete;
    ClExtensionPolicy &operator=(const ClExtensionPolicy &) = delete;

    size_t report(const ExtensionWords &host, char *into, size_t into_bytes) const;
    size_t report_versions(const ExtensionWords &host, cl_name_version *into, size_t room) const;

    bool reports(const ExtensionWords &host, const char *name) const;
    bool reportable(const char *name) const;
    void observe_vendor(uint32_t vendor);

    uint32_t rows() const;

    bool invariants_hold() const;

private:
    friend struct ClExtensionPolicyProbe;

    bool     answered_by(const ExtensionWords &host, uint32_t index) const;
    uint32_t vendor_of(const char *host) const;

    icd::ExtensionRows rows_;
    uint32_t           vendor_;
};

extern const ClExtensionPolicy::Row reported_extensions[];
extern const uint32_t                reported_extension_count;

}

#endif
