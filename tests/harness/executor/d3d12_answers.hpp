#ifndef AV_D3D12_ANSWERS_HPP
#define AV_D3D12_ANSWERS_HPP

#include "proto/generated/d3d12/d3d12_dispatch_client.h"

#include <cstdint>

namespace av::d3d12proto {

inline bool av_d3d12_CreateHeap12(ClientTransport &t, std::uint32_t heap, const HeapProperties &properties,
                                  std::uint32_t flags, std::uint64_t size, std::uint64_t alignment,
                                  std::uint64_t memory, std::uint64_t offset)
{
    std::uint32_t hresult = 1u;
    return av_d3d12_CreateHeap12(t, heap, properties, flags, size, alignment, memory, offset, &hresult) && hresult == 0u;
}

inline bool av_d3d12_CreateCommittedResource12(ClientTransport &t, std::uint32_t resource,
                                               const HeapProperties &properties, std::uint32_t heap_flags,
                                               const ResourceDesc &desc, std::uint32_t state, const ClearValue &clear,
                                               std::uint64_t address)
{
    std::uint32_t hresult = 1u;
    return av_d3d12_CreateCommittedResource12(t, resource, properties, heap_flags, desc, state, clear, address,
                                              &hresult) &&
           hresult == 0u;
}

}

#endif
