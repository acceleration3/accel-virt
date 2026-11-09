#include "icd/opengl/preferred_adapter.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "icd/opengl/adapter_order.hpp"

namespace av::glicd {

PreferredAdapter::PreferredAdapter(Kmt &kmt, AdapterOrder &order) : kmt_(kmt), order_(order)
{
    AV_SCOPE("PreferredAdapter::PreferredAdapter");
}

uint64_t PreferredAdapter::choose(void *dc)
{
    AV_SCOPE("PreferredAdapter::choose");
    const uint64_t first = order_.listed_first();
    if (first != 0u && avgpu_has(first)) return first;
    const uint64_t drawn = order_.drawn_on(dc);
    if (drawn != 0u && avgpu_has(drawn)) return drawn;
    return 0u;
}

bool PreferredAdapter::avgpu_has(uint64_t luid)
{
    AV_SCOPE("PreferredAdapter::avgpu_has");
    AvgpuAdapters adapters(kmt_);
    if (adapters.learn() != Status::ok) return false;
    const uint32_t index = adapters.index_of(luid);
    return index < adapters.count();
}

}
