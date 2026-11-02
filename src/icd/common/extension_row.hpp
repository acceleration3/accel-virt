#ifndef AV_ICD_EXTENSION_ROW_HPP
#define AV_ICD_EXTENSION_ROW_HPP

#include <stdint.h>

namespace av::icd {

struct ExtensionRow {
    const char *name_;
    uint32_t    spec_version_;
    const char *test_;
    const char *host_ = nullptr;
};

}

#endif
