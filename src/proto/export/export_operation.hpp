#ifndef AV_EXPORT_OPERATION_HPP
#define AV_EXPORT_OPERATION_HPP

#include <stdint.h>

namespace av {

enum class ExportOperation : uint32_t {
    unreadable = 0u,
    hello = 1u,
    export_surface = 2u,
    sync = 3u,
    release = 4u,
    timing = 5u,
    head = 6u,
    revoked = 0x80u
};

enum class ExportRevocation : uint32_t {
    released = 1u,
    reset = 2u
};

}

#endif
