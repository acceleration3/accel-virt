#include "device/memory/hostmem_space.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

HostmemSpace::HostmemSpace(Host &host) : host_(host)
{
    AV_SCOPE("HostmemSpace::HostmemSpace");
}

bool HostmemSpace::admits(uint64_t offset, uint64_t size) const
{
    AV_SCOPE("HostmemSpace::admits");
    const uint64_t region = host_.hostmem_size();
    return offset % page_size == 0u && size != 0u && offset <= region && size <= region - offset;
}

}
