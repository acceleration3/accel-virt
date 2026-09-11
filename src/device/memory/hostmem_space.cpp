#include "device/memory/hostmem_space.hpp"

#include "device/host/host.hpp"

namespace av {

HostmemSpace::HostmemSpace(Host &host) : host_(host) {}

bool HostmemSpace::admits(uint64_t offset, uint64_t size) const
{
    const uint64_t region = host_.hostmem_size();
    return offset % page_size == 0u && size != 0u && offset <= region && size <= region - offset;
}

}  // namespace av
