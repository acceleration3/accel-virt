#include "executor/d3d/d3d12_query_heaps.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t heap_occlusion = 0u;
constexpr uint32_t heap_timestamp = 1u;
constexpr uint32_t heap_pipeline_statistics = 2u;
constexpr uint32_t heap_stream_output = 3u;
constexpr uint32_t heap_copy_timestamp = 5u;
constexpr uint32_t query_occlusion = 0u;
constexpr uint32_t query_binary_occlusion = 1u;
constexpr uint32_t query_timestamp = 2u;
constexpr uint32_t query_pipeline_statistics = 3u;
constexpr uint32_t query_first_stream = 4u;
constexpr uint32_t query_last_stream = 7u;

}

D3d12QueryHeaps::D3d12QueryHeaps(BlockSource &blocks) : heaps_(blocks, Pool::paged)
{
    AV_SCOPE("D3d12QueryHeaps::D3d12QueryHeaps");
}

Status D3d12QueryHeaps::remember(uint32_t heap, uint32_t heap_type, uint32_t count)
{
    AV_SCOPE("D3d12QueryHeaps::remember");
    uint32_t slot = 0u;
    return heaps_.insert(heap, Info{heap_type, count}, slot);
}

void D3d12QueryHeaps::forget(uint32_t heap)
{
    AV_SCOPE("D3d12QueryHeaps::forget");
    static_cast<void>(heaps_.erase(heap));
}

void D3d12QueryHeaps::clear()
{
    AV_SCOPE("D3d12QueryHeaps::clear");
    heaps_.clear();
}

bool D3d12QueryHeaps::admits(uint32_t heap, uint32_t query_type, uint32_t first, uint32_t count) const
{
    AV_SCOPE("D3d12QueryHeaps::admits");
    const Info *const info = heaps_.find(heap);
    if (info == nullptr || first > info->count_ || count > info->count_ - first) return false;
    switch (info->type_) {
    case heap_occlusion: return query_type == query_occlusion || query_type == query_binary_occlusion;
    case heap_timestamp:
    case heap_copy_timestamp: return query_type == query_timestamp;
    case heap_pipeline_statistics: return query_type == query_pipeline_statistics;
    case heap_stream_output: return query_type >= query_first_stream && query_type <= query_last_stream;
    default: return false;
    }
}

uint32_t D3d12QueryHeaps::known() const
{
    AV_SCOPE("D3d12QueryHeaps::known");
    return heaps_.count();
}

bool D3d12QueryHeaps::invariants_hold() const
{
    AV_SCOPE("D3d12QueryHeaps::invariants_hold");
    return heaps_.invariants_hold();
}

}
