#ifndef AV_ICD_VERSION_TEXT_HPP
#define AV_ICD_VERSION_TEXT_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

class VersionText {
public:
    VersionText(const char *prefix, uint32_t most_major, uint32_t most_minor);
    VersionText(const VersionText &) = delete;
    VersionText &operator=(const VersionText &) = delete;

    [[nodiscard]] Status read(const char *host, size_t bytes, const char *&rest);
    void                 cap(uint32_t major, uint32_t minor);
    uint32_t             major() const;
    uint32_t             minor() const;
    void                 write(char *into, size_t bytes, const char *tail) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t most_digits = 3u;

    bool read_number(const char *host, size_t &at, size_t end, uint32_t &number) const;
    bool at_most_implemented(uint32_t major, uint32_t minor) const;

    const char *prefix_;
    uint32_t    most_major_;
    uint32_t    most_minor_;
    uint32_t    major_;
    uint32_t    minor_;
};

}

#endif
