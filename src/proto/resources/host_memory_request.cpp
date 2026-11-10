#include "proto/resources/host_memory_request.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HostMemoryRequest::HostMemoryRequest() : encoded_(0u)
{
    AV_SCOPE("HostMemoryRequest::HostMemoryRequest");
}

HostMemoryRequest::HostMemoryRequest(Kind kind, uint32_t vulkan_type)
    : encoded_(static_cast<uint32_t>(kind) | vulkan_type << type_shift)
{
    AV_SCOPE("HostMemoryRequest::HostMemoryRequest");
}

HostMemoryRequest::HostMemoryRequest(uint32_t encoded) : encoded_(encoded)
{
    AV_SCOPE("HostMemoryRequest::HostMemoryRequest");
}

bool HostMemoryRequest::valid() const
{
    AV_SCOPE("HostMemoryRequest::valid");
    const uint32_t kind = encoded_ & kind_mask;
    const uint32_t type = encoded_ >> type_shift;
    if (kind == static_cast<uint32_t>(Kind::vulkan) || kind == static_cast<uint32_t>(Kind::placed))
        return type < memory_types;
    return (kind == static_cast<uint32_t>(Kind::upload) || kind == static_cast<uint32_t>(Kind::readback) ||
            kind == static_cast<uint32_t>(Kind::placed_heap)) &&
           type == 0u;
}

bool HostMemoryRequest::places() const
{
    AV_SCOPE("HostMemoryRequest::places");
    const Kind asked = kind();
    return asked == Kind::placed || asked == Kind::placed_heap;
}

HostMemoryRequest::Kind HostMemoryRequest::kind() const
{
    AV_SCOPE("HostMemoryRequest::kind");
    return static_cast<Kind>(encoded_ & kind_mask);
}

uint32_t HostMemoryRequest::vulkan_type() const
{
    AV_SCOPE("HostMemoryRequest::vulkan_type");
    return encoded_ >> type_shift;
}

uint32_t HostMemoryRequest::encoded() const
{
    AV_SCOPE("HostMemoryRequest::encoded");
    return encoded_;
}

}
