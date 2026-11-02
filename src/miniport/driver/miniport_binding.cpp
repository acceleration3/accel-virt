#include "miniport/driver/miniport_binding.hpp"

#include "miniport/driver/image_exit.hpp"
#include "miniport/driver/miniport.hpp"
#include "core/support/nt_status.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

void      MiniportBinding::bind(Miniport &miniport, ImageExit *exit)
{
    AV_SCOPE("MiniportBinding::bind");
    miniport_ = &miniport;
    exit_ = exit;
}
void      MiniportBinding::unbind()
{
    AV_SCOPE("MiniportBinding::unbind");
    miniport_ = nullptr;
    exit_ = nullptr;
}
Miniport *MiniportBinding::miniport() const
{
    AV_SCOPE("MiniportBinding::miniport");
    return miniport_;
}

int32_t MiniportBinding::add_device(void *physical_device_object, void **context)
{
    AV_SCOPE("MiniportBinding::add_device");
    *context = nullptr;
    if (miniport_ == nullptr) return NtStatus(Status::invalid_state).value();
    void *created = nullptr;
    const Status status = miniport_->add_device(physical_device_object, created);
    *context = created;
    return NtStatus(status).value();
}

int32_t MiniportBinding::remove_device(void *context)
{
    AV_SCOPE("MiniportBinding::remove_device");
    if (miniport_ == nullptr) return NtStatus(Status::invalid_state).value();
    return NtStatus(miniport_->remove_device(context)).value();
}

void MiniportBinding::unload()
{
    AV_SCOPE("MiniportBinding::unload");
    if (miniport_ != nullptr) miniport_->unload();
    if (exit_ != nullptr) exit_->leave();
}

}
