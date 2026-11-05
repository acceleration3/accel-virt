#ifndef AV_CLIENT_BLOB_LEASE_HPP
#define AV_CLIENT_BLOB_LEASE_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClientBlobLease {
public:
    ClientBlobLease();
    ClientBlobLease(void *data, uint64_t offset, size_t bytes);

    bool     taken() const;
    void    *data() const;
    uint64_t offset() const;
    size_t   bytes() const;

private:
    void    *data_;
    uint64_t offset_;
    size_t   bytes_;
};

}

#endif
