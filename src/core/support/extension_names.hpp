#ifndef AV_EXTENSION_NAMES_HPP
#define AV_EXTENSION_NAMES_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class ExtensionNames {
public:
    ExtensionNames(BlockSource &blocks, Pool pool);
    ExtensionNames(const ExtensionNames &) = delete;
    ExtensionNames &operator=(const ExtensionNames &) = delete;

    [[nodiscard]] Status take(const char *const *names, uint32_t count, uint32_t adding);
    [[nodiscard]] Status reserve(uint32_t count);
    [[nodiscard]] Status add(const char *name);
    bool                 holds(const char *name) const;
    uint32_t             count() const;
    const char *const   *names() const;
    const char          *at(uint32_t index) const;
    void                 clear();

    bool invariants_hold() const;

private:
    friend struct ExtensionNamesProbe;

    using Names = GrowableArray<const char *>;

    bool same(const char *one, const char *other) const;

    Names names_;
};

}

#endif
