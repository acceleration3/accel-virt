#include "executor/d3d/d3d11_batch_progress.hpp"

#include "core/trace/function_scope.hpp"

#include "executor/eventfd_wake.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d11BatchProgress::D3d11BatchProgress(EventfdWake &wake)
    : wake_(wake), fence_(nullptr), marked_(0u), released_at_(0u), watched_(false)
{
    AV_SCOPE("D3d11BatchProgress::D3d11BatchProgress");
}

D3d11BatchProgress::~D3d11BatchProgress()
{
    AV_SCOPE("D3d11BatchProgress::~D3d11BatchProgress");
    release();
}

void D3d11BatchProgress::adopt(ID3D11Fence *fence)
{
    AV_SCOPE("D3d11BatchProgress::adopt");
    if (fence == nullptr) return;
    if (fence_ != nullptr) {
        fence->Release();
        return;
    }
    fence_ = fence;
}

ID3D11Fence *D3d11BatchProgress::fence() const
{
    AV_SCOPE("D3d11BatchProgress::fence");
    return fence_;
}

uint64_t D3d11BatchProgress::next() const
{
    AV_SCOPE("D3d11BatchProgress::next");
    return marked_ + 1u;
}

void D3d11BatchProgress::signalled()
{
    AV_SCOPE("D3d11BatchProgress::signalled");
    if (fence_ == nullptr) return;
    marked_++;
    watched_ = wake_.watch(*fence_, marked_);
}

void D3d11BatchProgress::release()
{
    AV_SCOPE("D3d11BatchProgress::release");
    if (fence_ == nullptr) return;
    fence_->Release();
    fence_ = nullptr;
    released_at_ = marked_;
    watched_ = false;
}

uint64_t D3d11BatchProgress::marked() const
{
    AV_SCOPE("D3d11BatchProgress::marked");
    return marked_;
}

/* A device lost answers UINT64_MAX: nothing more will finish, and nothing
 * marked is waited for. */
uint64_t D3d11BatchProgress::reached() const
{
    AV_SCOPE("D3d11BatchProgress::reached");
    if (fence_ == nullptr) return released_at_;
    const uint64_t completed = fence_->GetCompletedValue();
    return completed > marked_ ? marked_ : completed;
}

bool D3d11BatchProgress::pending() const
{
    AV_SCOPE("D3d11BatchProgress::pending");
    return reached() < marked_;
}

bool D3d11BatchProgress::polled() const
{
    AV_SCOPE("D3d11BatchProgress::polled");
    return !watched_ && pending();
}

bool D3d11BatchProgress::invariants_hold() const
{
    AV_SCOPE("D3d11BatchProgress::invariants_hold");
    return released_at_ <= marked_ && (fence_ != nullptr || (released_at_ == marked_ && !watched_));
}

}  // namespace av
