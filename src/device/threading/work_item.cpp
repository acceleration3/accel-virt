#include "device/threading/work_item.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WorkItem::WorkItem() : work_(nullptr), completion_(nullptr), context_(nullptr)
{
    AV_SCOPE("WorkItem::WorkItem");
}

WorkItem::WorkItem(HostRoutine work, HostRoutine completion, void *context)
    : work_(work), completion_(completion), context_(context)
    {
        AV_SCOPE("WorkItem::WorkItem");
    }

void WorkItem::run() const
{
    AV_SCOPE("WorkItem::run");
    work_(context_);
}

void WorkItem::complete_on(Host &host) const
{
    AV_SCOPE("WorkItem::complete_on");
    if (completion_ != nullptr) host.run_on_main_loop(completion_, context_);
}

}
