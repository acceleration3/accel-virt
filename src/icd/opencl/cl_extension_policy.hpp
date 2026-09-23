/* Which OpenCL extensions the application is shown.
 *
 * An extension is reported only when a row here names it *and* the host device
 * reports it too: the row is what says this ICD carries it, and the host is
 * what says the GPU does. Every row names the test that proves the extension
 * through this ICD, which tools/check_extension_policy.py holds it to - no
 * test, no row; no row, no report (constitution VI).
 *
 * The device's answer is one space-separated string, unlike Vulkan's array of
 * structures, so only the row type is shared with icd::ExtensionPolicy - the
 * shape GlExtensionPolicy already takes for OpenGL's. */
#ifndef AV_CLICD_CL_EXTENSION_POLICY_HPP
#define AV_CLICD_CL_EXTENSION_POLICY_HPP

#include "icd/common/extension_policy.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

struct ClExtensionPolicyProbe;

class ClExtensionPolicy {
public:
    using Row = icd::ExtensionPolicy::Row;

    /* An extension name past this is not one any registry has, so a host
     * string carrying one is a host string this does not believe. */
    static constexpr uint32_t name_bytes = 128u;
    static constexpr uint32_t max_rows = 32u;

    /* `rows` is a constant table the owner keeps for as long as this. */
    ClExtensionPolicy(const Row *rows, uint32_t count);
    ClExtensionPolicy(const ClExtensionPolicy &) = delete;
    ClExtensionPolicy &operator=(const ClExtensionPolicy &) = delete;

    /* The reportable extensions of a device whose CL_DEVICE_EXTENSIONS is
     * `host` - space-separated, and not necessarily terminated within
     * `host_bytes` - written into `into` in the policy's order, space
     * separated and terminated. Answers the bytes the answer needs including
     * its terminator, whether or not it fitted; nothing is written when it
     * did not fit, or when `into` is null. */
    size_t report(const char *host, size_t host_bytes, char *into, size_t into_bytes) const;

    /* Whether the host string offers `name`, as a whole word. */
    bool offered_by(const char *host, size_t host_bytes, const char *name) const;
    /* Whether a row names it; what the ICD asks before it acts on one. */
    bool reportable(const char *name) const;

    uint32_t rows() const;

    bool invariants_hold() const;

private:
    friend struct ClExtensionPolicyProbe;

    /* Whether a host offering `host` answers `row`: its own name, or the host
     * extension the row names instead. */
    bool answered_by(const char *host, size_t host_bytes, const Row &row) const;

    const Row *rows_;
    uint32_t   count_;
};

/* What this ICD ships, read by tools/check_extension_policy.py and by the
 * root. */
extern const ClExtensionPolicy::Row reported_extensions[];
extern const uint32_t                reported_extension_count;

}  // namespace av::clicd

#endif  // AV_CLICD_CL_EXTENSION_POLICY_HPP
