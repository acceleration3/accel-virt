/* One driver's table of reportable extensions, matched against a host's names:
 * the rows Vulkan's ExtensionPolicy, OpenCL's ClExtensionPolicy and OpenGL's
 * GlExtensionPolicy (its GL and its WGL rows) each report from, which
 * tools/check_extension_policy.py holds to a test per row (constitution VI).
 *
 * A row is reportable when what the generator carried names it - a row the
 * generator dropped has no commands behind it - and no withheld name is it:
 * an extension the driver answers otherwise whatever a row says (Vulkan's
 * surfaces, presented by the ICD itself; WGL's tear control, an interval no
 * pacing here honours). The driver's own rows - WGL's functions, which no
 * generator lists - are carried by the driver itself.
 *
 * How a host's names arrive, and how the answer is spelled, is each policy's:
 * an array of VkExtensionProperties, one space-separated string, or a name per
 * glGetStringi. A name from outside is compared no further than a row's own
 * length and its terminator. */
#ifndef AV_ICD_EXTENSION_ROWS_HPP
#define AV_ICD_EXTENSION_ROWS_HPP

#include "icd/common/extension_row.hpp"

#include <stdint.h>

namespace av::icd {

class ExtensionRows {
public:
    /* No registry names an extension this long; VkExtensionProperties holds
     * one this wide, terminator included. */
    static constexpr uint32_t name_bytes = 256u;

    /* A constant list of names the owner keeps for as long as this. */
    class Names {
    public:
        Names(const char *const *names, uint32_t count);

        /* Whether one of them is `name`, compared whole. */
        bool holds(const char *name) const;

    private:
        const char *const *names_;
        uint32_t           count_;
    };

    /* Rows of extensions a generator carries: `count` rows at `rows`, every
     * name starting with `prefix`. All are constants the owner keeps. */
    ExtensionRows(const ExtensionRow *rows, uint32_t count, const char *prefix, Names carried, Names withheld);
    /* Rows of the driver's own extensions, which it carries itself. */
    ExtensionRows(const ExtensionRow *rows, uint32_t count, const char *prefix, Names withheld);
    ExtensionRows(const ExtensionRows &) = delete;
    ExtensionRows &operator=(const ExtensionRows &) = delete;

    uint32_t            count() const;
    /* The `index`th row, `index` below count(). */
    const ExtensionRow &row(uint32_t index) const;
    /* Whether the `index`th row may be reported at all: carried and not
     * withheld. */
    bool                reportable(uint32_t index) const;
    /* The reportable row named `name`, or count() - for a null name too. */
    uint32_t            named(const char *name) const;
    /* The first reportable row from `from` on that a host listing `host_name`
     * answers, or count(): several rows may be answered over one host
     * extension, as GL_EXT_memory_object and GL_EXT_memory_object_win32 both
     * are over GL_EXT_memory_object_fd. */
    uint32_t            answering(const char *host_name, uint32_t from) const;
    /* The name a host must list for the `index`th row: the host extension it
     * is answered over, or its own. */
    const char         *host_name(uint32_t index) const;
    /* The version the `index`th row is reported at over a host's
     * `host_version`: the row's caps the host's, since a newer revision was
     * not what was tested, and a row answered over another extension is
     * reported at its own, since the host's version is a different
     * extension's. */
    uint32_t            version(uint32_t index, uint32_t host_version) const;

    /* Every row names an extension starting with the prefix in fewer than
     * name_bytes, a test and a version, and a host extension only by a
     * non-empty name; no name twice. */
    bool invariants_hold() const;

private:
    /* Whether `name` is `ours` whole, reading no further of `name` than
     * `ours` and its terminator. */
    bool     same(const char *ours, const char *name) const;
    /* The letters of `name`, at most name_bytes. */
    uint32_t letters_of(const char *name) const;
    bool     row_holds(const ExtensionRow &row) const;

    const ExtensionRow *rows_;
    uint32_t            count_;
    const char         *prefix_;
    Names               carried_;
    Names               withheld_;
    /* The driver's own rows: no carried list judges them. */
    bool                own_;
};

}  // namespace av::icd

#endif  // AV_ICD_EXTENSION_ROWS_HPP
