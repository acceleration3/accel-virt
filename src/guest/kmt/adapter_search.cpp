#include "guest/kmt/adapter_search.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

AdapterSearch::AdapterSearch(Kmt &kmt) : kmt_(kmt), adapter_(0u)
{
    AV_SCOPE("AdapterSearch::AdapterSearch");
}

AdapterSearch::~AdapterSearch()
{
    AV_SCOPE("AdapterSearch::~AdapterSearch");
    static_cast<void>(close());
}

Status AdapterSearch::find_avgpu(uint64_t luid)
{
    AV_SCOPE("AdapterSearch::find_avgpu");
    if (adapter_ != 0u) return Status::invalid_state;
    AvgpuAdapters adapters(kmt_);
    const Status  learnt = adapters.learn();
    if (learnt != Status::ok) return learnt;
    adapter_ = adapters.take(luid != 0u ? adapters.index_of(luid) : 0u);
    return adapter_ != 0u ? Status::ok : Status::not_found;
}

Status AdapterSearch::close()
{
    AV_SCOPE("AdapterSearch::close");
    const uint32_t adapter = adapter_;
    adapter_ = 0u;
    return adapter != 0u ? kmt_.close_adapter(adapter) : Status::ok;
}

bool AdapterSearch::found() const
{
    AV_SCOPE("AdapterSearch::found");
    return adapter_ != 0u;
}

uint32_t AdapterSearch::adapter() const
{
    AV_SCOPE("AdapterSearch::adapter");
    return adapter_;
}

}  // namespace av
