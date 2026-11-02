#ifndef AV_RECORD_KIND_HPP
#define AV_RECORD_KIND_HPP

#include <stdint.h>

namespace av {

enum class RecordKind : uint16_t {
    context_setup   = 1u,
    batch           = 2u,
    reply           = 3u,
    fence_completed = 4u,
    fault           = 5u,
    blob            = 6u,
    blob_bytes      = 7u,
};

}

#endif
