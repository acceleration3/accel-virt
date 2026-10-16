#include "icd/common/context_link.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/device_link.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/icd_connection.hpp"

namespace av::icd {

ContextLink::ContextLink(IcdConnection &connection) : connection_(connection), link_(nullptr)
{
    AV_SCOPE("ContextLink::ContextLink");
}

ContextLink::~ContextLink()
{
    AV_SCOPE("ContextLink::~ContextLink");
    release();
}

Status ContextLink::acquire()
{
    AV_SCOPE("ContextLink::acquire");
    if (held()) return Status::invalid_state;
    return connection_.acquire(link_);
}

void ContextLink::release()
{
    AV_SCOPE("ContextLink::release");
    if (!held()) return;
    connection_.release(*link_);
    link_ = nullptr;
}

bool ContextLink::on(const IcdConnection &connection) const
{
    AV_SCOPE("ContextLink::on");
    return &connection_ == &connection;
}

bool ContextLink::held() const
{
    AV_SCOPE("ContextLink::held");
    return link_ != nullptr;
}

GuestConnection &ContextLink::device() const
{
    AV_SCOPE("ContextLink::device");
    return link_->device();
}

bool ContextLink::lost(const HostChannel &channel)
{
    AV_SCOPE("ContextLink::lost");
    if (!held()) return false;
    if (channel.lost()) link_->lose();
    return link_->lost();
}

}  // namespace av::icd
