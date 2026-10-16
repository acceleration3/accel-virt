/* Which OpenCL extensions the application is shown.
 *
 * An extension is reported only when a row here names it *and* the host device
 * reports it too: the row is what says this ICD carries it, and the host is
 * what says the GPU does. The exception is a row whose host is `answered_here`:
 * an extension this ICD answers alone, about guest objects no host GPU has,
 * which is reported whatever the host offers - or, for a host of
 * `answered_here` followed by a PCI vendor ID in hex ("-1002"), reported on an
 * adapter of that vendor alone: a vendor's own attribute query, which this ICD
 * answers with where the guest's adapter sits; followed by "*", reported once
 * the adapter is described at all. Every row names the test that proves the extension
 * through this ICD, which tools/check_extension_policy.py holds it to - no
 * test, no row; no row, no report (constitution VI).
 *
 * The device's answer is one space-separated string, unlike Vulkan's array of
 * structures, so only the row type is shared with Vulkan's ExtensionPolicy - the
 * shape GlExtensionPolicy already takes for OpenGL's. */
#ifndef AV_CLICD_CL_EXTENSION_POLICY_HPP
#define AV_CLICD_CL_EXTENSION_POLICY_HPP

#include "icd/common/extension_row.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {
class ExtensionWords;
}  // namespace av

namespace av::clicd {

struct ClExtensionPolicyProbe;

class ClExtensionPolicy {
public:
    using Row = icd::ExtensionRow;

    /* An extension name past this is not one any registry has, so a host
     * string carrying one is a host string this does not believe. */
    static constexpr uint32_t name_bytes = 128u;
    static constexpr uint32_t max_rows = 32u;
    /* The host a row names when no host extension answers it: the ICD does. */
    static constexpr char answered_here[] = "-";

    /* `rows` is a constant table the owner keeps for as long as this. */
    ClExtensionPolicy(const Row *rows, uint32_t count);
    ClExtensionPolicy(const ClExtensionPolicy &) = delete;
    ClExtensionPolicy &operator=(const ClExtensionPolicy &) = delete;

    /* The reportable extensions of a device whose CL_DEVICE_EXTENSIONS is
     * `host`, written into `into` in the policy's order, space separated and
     * terminated. Answers the bytes the answer needs including its
     * terminator, whether or not it fitted; nothing is written when it did
     * not fit, or when `into` is null. */
    size_t report(const ExtensionWords &host, char *into, size_t into_bytes) const;
    /* The same extensions, in the same order, as CL_DEVICE_EXTENSIONS_WITH_VERSION
     * names them: each with its row's version, which is the version this ICD
     * carries whatever the host's own is. Answers how many there are, whether
     * or not they fitted in `room` entries; nothing is written when they did
     * not, or when `into` is null. */
    size_t report_versions(const ExtensionWords &host, cl_name_version *into, size_t room) const;

    /* Whether report lists `name` for a device whose host offers `host`. */
    bool reports(const ExtensionWords &host, const char *name) const;
    /* Whether a row names it; what the ICD asks before it acts on one. */
    bool reportable(const char *name) const;
    /* The PCI vendor the device's adapter presents, which a vendor's own rows
     * are reported on; none until this. */
    void observe_vendor(uint32_t vendor);

    uint32_t rows() const;

    bool invariants_hold() const;

private:
    friend struct ClExtensionPolicyProbe;

    /* Whether a host offering `host` answers `row`: its own name, or the host
     * extension the row names instead. */
    bool     answered_by(const ExtensionWords &host, const Row &row) const;
    /* The letters of a row's name, at most name_bytes. */
    size_t   letters_of(const char *name) const;
    /* Whether a row's name is `name`. */
    bool     same(const char *row_name, const char *name) const;
    /* The PCI vendor an "-<hex>" row is answered on, from the hex after its
     * dash; 0 for anything that is not hex. */
    uint32_t vendor_of(const char *host) const;

    const Row *rows_;
    uint32_t   count_;
    uint32_t   vendor_;
};

/* What this ICD ships, read by tools/check_extension_policy.py and by the
 * root. */
extern const ClExtensionPolicy::Row reported_extensions[];
extern const uint32_t                reported_extension_count;

}  // namespace av::clicd

#endif  // AV_CLICD_CL_EXTENSION_POLICY_HPP
