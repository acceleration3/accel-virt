#ifndef AV_HOST_BLOB_OWNERS_HPP
#define AV_HOST_BLOB_OWNERS_HPP

#include <stdint.h>

namespace av {

class HostBlobOwners {
public:
    HostBlobOwners(const HostBlobOwners &) = delete;
    HostBlobOwners &operator=(const HostBlobOwners &) = delete;

    virtual bool owns(uint32_t context, uint32_t id) = 0;

protected:
    HostBlobOwners() = default;
    ~HostBlobOwners() = default;
};

}

#endif
