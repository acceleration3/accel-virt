#ifndef AV_GLICD_GL_EXTENSION_LIST_HPP
#define AV_GLICD_GL_EXTENSION_LIST_HPP

#include "core/support/extension_names.hpp"
#include "core/support/extension_words.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::glicd {

struct GlExtensionListProbe;

class GlExtensionList {
public:
    explicit GlExtensionList(BlockSource &memory);
    GlExtensionList(const GlExtensionList &) = delete;
    GlExtensionList &operator=(const GlExtensionList &) = delete;

    bool        add(const char *name);
    uint32_t    count() const;
    const char *name(uint32_t index) const;
    const char *joined() const;

    bool invariants_hold() const;

private:
    ExtensionNames names_;
    ExtensionWords joined_;

    friend struct GlExtensionListProbe;
};

}

#endif
