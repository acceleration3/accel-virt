#include "device/threading/work_item.hpp"

namespace av {

WorkItem::WorkItem() : work_(nullptr), completion_(nullptr), context_(nullptr) {}

WorkItem::WorkItem(HostRoutine work, HostRoutine completion, void *context)
    : work_(work), completion_(completion), context_(context) {}

void WorkItem::run() const
{
    work_(context_);
}

void WorkItem::complete_on(Host &host) const
{
    host.run_on_main_loop(completion_, context_);
}

}  // namespace av
