/* The version an API's version string leads with, read from what the host
 * answered and capped at the version this driver implements: OpenCL's
 * CL_DEVICE_VERSION, CL_DEVICE_OPENCL_C_VERSION and CL_PLATFORM_VERSION,
 * OpenGL's GL_VERSION and GL_SHADING_LANGUAGE_VERSION.
 *
 * A host newer than this driver says what the host implements, not what an
 * application may call through this driver, and what is reported is what is
 * tested (constitution VI). Each string is "<prefix><major>.<minor>" and then
 * whatever its vendor says; the number is read whole, since reading one
 * digit would turn 12.0 into 1.0, and is rewritten while what follows it is
 * the caller's to keep or replace. */
#ifndef AV_ICD_VERSION_TEXT_HPP
#define AV_ICD_VERSION_TEXT_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

class VersionText {
public:
    /* `prefix` precedes the number - "OpenCL C ", or nothing for OpenGL - and
     * the driver implements `most_major`.`most_minor`, which is the version
     * until another is read. Both are constants the owner keeps. */
    VersionText(const char *prefix, uint32_t most_major, uint32_t most_minor);
    VersionText(const VersionText &) = delete;
    VersionText &operator=(const VersionText &) = delete;

    /* The version `host` leads with, capped: "<prefix><major>.<minor>" in its
     * first `bytes`, which hold its terminator. `rest` is where `host`
     * goes on after the number. malformed, leaving the version and `rest` as
     * they were, for a string of any other shape. */
    [[nodiscard]] Status read(const char *host, size_t bytes, const char *&rest);
    /* `major`.`minor` as the version, capped. */
    void                 cap(uint32_t major, uint32_t minor);
    uint32_t             major() const;
    uint32_t             minor() const;
    /* "<prefix><major>.<minor><tail>" into the `bytes` at `into`, cut where
     * they end and terminated. */
    void                 write(char *into, size_t bytes, const char *tail) const;

    /* The version is at most the implemented one. */
    bool invariants_hold() const;

private:
    /* A number longer than this is no version any API has had. */
    static constexpr uint32_t most_digits = 3u;

    /* The decimal number at `at`, `at` moved past it; false for none, or one
     * of more than most_digits. */
    bool read_number(const char *host, size_t &at, size_t end, uint32_t &number) const;
    bool at_most_implemented(uint32_t major, uint32_t minor) const;

    const char *prefix_;
    uint32_t    most_major_;
    uint32_t    most_minor_;
    uint32_t    major_;
    uint32_t    minor_;
};

}  // namespace av::icd

#endif  // AV_ICD_VERSION_TEXT_HPP
