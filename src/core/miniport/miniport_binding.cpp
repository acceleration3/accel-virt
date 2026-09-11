#include "core/miniport/miniport_binding.hpp"

#include "core/miniport/miniport.hpp"
#include "core/support/nt_status.hpp"

namespace av {

void      MiniportBinding::bind(Miniport &miniport) { miniport_ = &miniport; }
void      MiniportBinding::unbind()                 { miniport_ = nullptr; }
Miniport *MiniportBinding::miniport() const         { return miniport_; }

int32_t MiniportBinding::add_device(void *physical_device_object, void **context)
{
    *context = nullptr;
    if (miniport_ == nullptr) return NtStatus(Status::invalid_state).value();
    void *created = nullptr;
    const Status status = miniport_->add_device(physical_device_object, created);
    *context = created;
    return NtStatus(status).value();
}

int32_t MiniportBinding::remove_device(void *context)
{
    if (miniport_ == nullptr) return NtStatus(Status::invalid_state).value();
    return NtStatus(miniport_->remove_device(context)).value();
}

void MiniportBinding::unload()
{
    if (miniport_ != nullptr) miniport_->unload();
}

}  // namespace av
