#include "executor/opengl/gl_batch_progress.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/shim/gl_native_fences.hpp"

#include <poll.h>
#include <unistd.h>

namespace av {

namespace {

constexpr uint32_t gl_sync_gpu_commands_complete = 0x9117u;
constexpr uint32_t gl_sync_flush_commands_bit = 0x1u;
constexpr uint32_t gl_timeout_expired = 0x911Bu;

}

GlBatchProgress::GlBatchProgress(const glproto::RealGl &gl, const GlNativeFences &natives, EventfdWake &wake)
    : gl_(gl), natives_(natives), wake_(wake), pending_{}, first_(0u), count_(0u), marked_(0u), reached_(0u)
{
    AV_SCOPE("GlBatchProgress::GlBatchProgress");
}

bool GlBatchProgress::glsync_fences() const
{
    AV_SCOPE("GlBatchProgress::glsync_fences");
    return gl_.glFenceSync != nullptr && gl_.glClientWaitSync != nullptr && gl_.glDeleteSync != nullptr;
}

bool GlBatchProgress::fences() const
{
    AV_SCOPE("GlBatchProgress::fences");
    return natives_.available() || glsync_fences();
}

void GlBatchProgress::drop(Pending &row)
{
    AV_SCOPE("GlBatchProgress::drop");
    if (row.sync_ != nullptr && gl_.glDeleteSync != nullptr) gl_.glDeleteSync(static_cast<GLsync>(row.sync_));
    if (row.fd_ >= 0) close(row.fd_);
    row = Pending{nullptr, -1, false, 0u};
}

bool GlBatchProgress::signalled(const Pending &row) const
{
    AV_SCOPE("GlBatchProgress::signalled");
    if (row.fd_ >= 0) {
        pollfd waiting = {row.fd_, POLLIN, 0};
        return poll(&waiting, 1u, 0) != 0;
    }
    return gl_.glClientWaitSync(static_cast<GLsync>(row.sync_), gl_sync_flush_commands_bit, 0u) != gl_timeout_expired;
}

void GlBatchProgress::fence()
{
    AV_SCOPE("GlBatchProgress::fence");
    if (!fences()) return;
    Pending made = {nullptr, natives_.make(), false, marked_ + 1u};
    if (made.fd_ >= 0) {
        made.watched_ = wake_.watch_descriptor(made.fd_);
    } else {
        if (!glsync_fences()) return;
        made.sync_ = gl_.glFenceSync(gl_sync_gpu_commands_complete, 0u);
        if (made.sync_ == nullptr) {
            if (gl_.glGetError != nullptr) static_cast<void>(gl_.glGetError());
            return;
        }
    }
    if (count_ == most_pending) {
        Pending &newest = pending_[(first_ + count_ - 1u) % most_pending];
        drop(newest);
        newest = made;
    } else {
        pending_[(first_ + count_) % most_pending] = made;
        count_++;
    }
    __atomic_store_n(&marked_, made.mark_, __ATOMIC_RELEASE);
}

bool GlBatchProgress::settle()
{
    AV_SCOPE("GlBatchProgress::settle");
    while (count_ != 0u) {
        Pending &oldest = pending_[first_];
        if (!signalled(oldest)) break;
        __atomic_store_n(&reached_, oldest.mark_, __ATOMIC_RELEASE);
        drop(oldest);
        first_ = (first_ + 1u) % most_pending;
        count_--;
    }
    return count_ != 0u;
}

bool GlBatchProgress::polled() const
{
    AV_SCOPE("GlBatchProgress::polled");
    for (uint32_t at = 0u; at < count_; at++)
        if (!pending_[(first_ + at) % most_pending].watched_) return true;
    return false;
}

void GlBatchProgress::release()
{
    AV_SCOPE("GlBatchProgress::release");
    while (count_ != 0u) {
        drop(pending_[first_]);
        first_ = (first_ + 1u) % most_pending;
        count_--;
    }
    __atomic_store_n(&reached_, __atomic_load_n(&marked_, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
}

uint64_t GlBatchProgress::next() const
{
    AV_SCOPE("GlBatchProgress::next");
    return __atomic_load_n(&marked_, __ATOMIC_ACQUIRE) + 1u;
}

uint64_t GlBatchProgress::reached() const
{
    AV_SCOPE("GlBatchProgress::reached");
    return __atomic_load_n(&reached_, __ATOMIC_ACQUIRE);
}

uint64_t GlBatchProgress::marked() const
{
    AV_SCOPE("GlBatchProgress::marked");
    return __atomic_load_n(&marked_, __ATOMIC_ACQUIRE);
}

bool GlBatchProgress::pending() const
{
    AV_SCOPE("GlBatchProgress::pending");
    return reached() < marked();
}

bool GlBatchProgress::invariants_hold() const
{
    AV_SCOPE("GlBatchProgress::invariants_hold");
    if (count_ > most_pending || first_ >= most_pending || reached_ > marked_) return false;
    uint64_t previous = reached_;
    for (uint32_t at = 0u; at < count_; at++) {
        const Pending &row = pending_[(first_ + at) % most_pending];
        if ((row.sync_ == nullptr) == (row.fd_ < 0) || row.mark_ <= previous || row.mark_ > marked_) return false;
        previous = row.mark_;
    }
    return previous == marked_ && (count_ == 0u) == (reached_ == marked_);
}

}
