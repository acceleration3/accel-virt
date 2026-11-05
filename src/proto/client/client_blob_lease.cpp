#include "proto/client/client_blob_lease.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClientBlobLease::ClientBlobLease() : data_(nullptr), offset_(0u), bytes_(0u)
{
    AV_SCOPE("ClientBlobLease::ClientBlobLease");
}

ClientBlobLease::ClientBlobLease(void *data, uint64_t offset, size_t bytes)
    : data_(data), offset_(offset), bytes_(bytes)
    {
        AV_SCOPE("ClientBlobLease::ClientBlobLease");
    }

bool     ClientBlobLease::taken() const
{
    AV_SCOPE("ClientBlobLease::taken");
    return data_ != nullptr;
}
void    *ClientBlobLease::data() const
{
    AV_SCOPE("ClientBlobLease::data");
    return data_;
}
uint64_t ClientBlobLease::offset() const
{
    AV_SCOPE("ClientBlobLease::offset");
    return offset_;
}
size_t   ClientBlobLease::bytes() const
{
    AV_SCOPE("ClientBlobLease::bytes");
    return bytes_;
}

}
