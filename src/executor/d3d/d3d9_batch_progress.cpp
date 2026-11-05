#include "executor/d3d/d3d9_batch_progress.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d9BatchProgress::D3d9BatchProgress() : slots_{}, first_(0u), used_(0u), marked_(0u), reaped_(0u)
{
    AV_SCOPE("D3d9BatchProgress::D3d9BatchProgress");
}

D3d9BatchProgress::~D3d9BatchProgress()
{
    AV_SCOPE("D3d9BatchProgress::~D3d9BatchProgress");
    release();
}

bool D3d9BatchProgress::signalled(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9BatchProgress::signalled");
    reap();
    const bool fresh = used_ < in_flight;
    Slot &slot = at(fresh ? used_ : used_ - 1u);
    if (!fresh && slot.device_ != device) return false;
    if (!prepare(slot, device) || FAILED(slot.query_->Issue(D3DISSUE_END))) return false;
    if (fresh) used_++;
    marked_++;
    slot.mark_ = marked_;
    flush(device);
    return true;
}

void D3d9BatchProgress::forget(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9BatchProgress::forget");
    for (Slot &slot : slots_) {
        if (slot.device_ != device) continue;
        if (slot.query_ != nullptr) slot.query_->Release();
        slot.query_ = nullptr;
        slot.device_ = nullptr;
    }
}

void D3d9BatchProgress::release()
{
    AV_SCOPE("D3d9BatchProgress::release");
    for (Slot &slot : slots_) {
        if (slot.query_ != nullptr) slot.query_->Release();
        slot = Slot{};
    }
    first_ = 0u;
    used_ = 0u;
    reaped_ = marked_;
}

uint64_t D3d9BatchProgress::next() const
{
    AV_SCOPE("D3d9BatchProgress::next");
    return marked_ + 1u;
}

uint64_t D3d9BatchProgress::marked() const
{
    AV_SCOPE("D3d9BatchProgress::marked");
    return marked_;
}

uint64_t D3d9BatchProgress::reached() const
{
    AV_SCOPE("D3d9BatchProgress::reached");
    uint64_t reached = reaped_;
    for (uint32_t index = 0u; index < used_ && done(at(index)); index++) reached = at(index).mark_;
    return reached;
}

bool D3d9BatchProgress::pending() const
{
    AV_SCOPE("D3d9BatchProgress::pending");
    return reached() < marked_;
}

void D3d9BatchProgress::reap()
{
    AV_SCOPE("D3d9BatchProgress::reap");
    while (used_ != 0u && done(at(0u))) {
        reaped_ = at(0u).mark_;
        first_ = (first_ + 1u) % in_flight;
        used_--;
    }
}

bool D3d9BatchProgress::done(const Slot &slot) const
{
    AV_SCOPE("D3d9BatchProgress::done");
    BOOL finished = FALSE;
    return slot.query_ == nullptr || slot.query_->GetData(&finished, sizeof(finished), 0u) == S_OK;
}

D3d9BatchProgress::Slot &D3d9BatchProgress::at(uint32_t index)
{
    AV_SCOPE("D3d9BatchProgress::at");
    return slots_[(first_ + index) % in_flight];
}

const D3d9BatchProgress::Slot &D3d9BatchProgress::at(uint32_t index) const
{
    AV_SCOPE("D3d9BatchProgress::at");
    return slots_[(first_ + index) % in_flight];
}

bool D3d9BatchProgress::prepare(Slot &slot, IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9BatchProgress::prepare");
    if (device == nullptr) return false;
    if (slot.query_ != nullptr && slot.device_ == device) return true;
    if (slot.query_ != nullptr) slot.query_->Release();
    slot.query_ = nullptr;
    slot.device_ = nullptr;
    if (FAILED(device->CreateQuery(D3DQUERYTYPE_EVENT, &slot.query_))) {
        slot.query_ = nullptr;
        return false;
    }
    slot.device_ = device;
    return true;
}

void D3d9BatchProgress::flush(IDirect3DDevice9 *device) const
{
    AV_SCOPE("D3d9BatchProgress::flush");
    ID3D9VkInteropDevice1 *interop = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(ID3D9VkInteropDevice1), reinterpret_cast<void **>(&interop)))) return;
    interop->FlushCommands();
    interop->Release();
}

bool D3d9BatchProgress::invariants_hold() const
{
    AV_SCOPE("D3d9BatchProgress::invariants_hold");
    if (used_ > in_flight || first_ >= in_flight || reaped_ > marked_) return false;
    if (used_ == 0u) return reaped_ == marked_;
    uint64_t previous = reaped_;
    for (uint32_t index = 0u; index < used_; index++) {
        if (at(index).mark_ <= previous) return false;
        previous = at(index).mark_;
    }
    return previous == marked_;
}

}
