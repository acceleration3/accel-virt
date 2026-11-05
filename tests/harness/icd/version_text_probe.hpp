#ifndef AV_VERSION_TEXT_PROBE_HPP
#define AV_VERSION_TEXT_PROBE_HPP

#include "icd/common/version_text.hpp"

#include <stdint.h>

namespace av::icd {

struct VersionTextProbe {
    static void set(VersionText &version, uint32_t major, uint32_t minor)
    {
        version.major_ = major;
        version.minor_ = minor;
    }
};

}

#endif
