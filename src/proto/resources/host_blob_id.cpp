#include "proto/resources/host_blob_id.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HostBlobId::HostBlobId(uint32_t id) : id_(id)
{
    AV_SCOPE("HostBlobId::HostBlobId");
}

bool HostBlobId::valid() const
{
    AV_SCOPE("HostBlobId::valid");
    return id_ >= first;
}

}  // namespace av
