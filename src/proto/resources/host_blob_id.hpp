#ifndef AV_HOST_BLOB_ID_HPP
#define AV_HOST_BLOB_ID_HPP

#include <stdint.h>

namespace av {

class HostBlobId {
public:
    static constexpr uint32_t first = 0x01000000u;
    static constexpr uint32_t most = 0u - first;

    explicit HostBlobId(uint32_t id);

    bool valid() const;

private:
    uint32_t id_;
};

}

#endif
