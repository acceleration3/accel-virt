#ifndef AV_ICD_HOST_EXTENSION_LIST_HPP
#define AV_ICD_HOST_EXTENSION_LIST_HPP

#include "core/support/extension_names.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::icd {

class HostExtensionList {
public:
    explicit HostExtensionList(BlockSource &memory);
    HostExtensionList(const HostExtensionList &) = delete;
    HostExtensionList &operator=(const HostExtensionList &) = delete;

    [[nodiscard]] Status take(const char *const *names, uint32_t count);
    [[nodiscard]] Status add(const char *name);
    bool                 named(const char *name) const;
    const char *const   *names() const;
    uint32_t             count() const;
    uint32_t             kept() const;

    bool invariants_hold() const;

private:
    bool is_the_icd_s(const char *name) const;
    const char *host_spelling_of(const char *name) const;

    ExtensionNames     names_;
    const char *const *given_names_;
    uint32_t           given_;
    uint32_t           kept_;
    uint32_t           added_;
};

}

#endif
