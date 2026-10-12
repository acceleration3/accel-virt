#include "icd/common/context_link.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/icd_connection.hpp"

namespace av::icd {

ContextLink::ContextLink(IcdConnection &connection) : connection_(connection), link_(IcdConnection::no_link)
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
    connection_.release(link_);
    link_ = IcdConnection::no_link;
}

bool ContextLink::on(const IcdConnection &connection) const
{
    AV_SCOPE("ContextLink::on");
    return &connection_ == &connection;
}

bool ContextLink::held() const
{
    AV_SCOPE("ContextLink::held");
    return link_ != IcdConnection::no_link;
}

GuestConnection &ContextLink::device() const
{
    AV_SCOPE("ContextLink::device");
    return connection_.device(link_);
}

bool ContextLink::lost(const HostChannel &channel)
{
    AV_SCOPE("ContextLink::lost");
    if (!held()) return false;
    if (channel.faulted()) connection_.lose(link_);
    return connection_.lost(link_);
}

uint32_t ContextLink::link() const
{
    AV_SCOPE("ContextLink::link");
    return link_;
}

}  // namespace av::icd
