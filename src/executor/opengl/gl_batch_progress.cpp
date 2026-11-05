#include "executor/opengl/gl_batch_progress.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t gl_sync_gpu_commands_complete = 0x9117u;
constexpr uint32_t gl_sync_flush_commands_bit = 0x1u;
constexpr uint32_t gl_timeout_expired = 0x911Bu;

}

GlBatchProgress::GlBatchProgress(const glproto::RealGl &gl)
    : gl_(gl), pending_{}, first_(0u), count_(0u), marked_(0u), reached_(0u)
{
    AV_SCOPE("GlBatchProgress::GlBatchProgress");
}

bool GlBatchProgress::fences() const
{
    AV_SCOPE("GlBatchProgress::fences");
    return gl_.glFenceSync != nullptr && gl_.glClientWaitSync != nullptr && gl_.glDeleteSync != nullptr;
}

void GlBatchProgress::drop_sync(void *sync)
{
    AV_SCOPE("GlBatchProgress::drop_sync");
    gl_.glDeleteSync(static_cast<GLsync>(sync));
}

void GlBatchProgress::fence()
{
    AV_SCOPE("GlBatchProgress::fence");
    if (!fences()) return;
    void *const sync = gl_.glFenceSync(gl_sync_gpu_commands_complete, 0u);
    if (sync == nullptr) {
        if (gl_.glGetError != nullptr) static_cast<void>(gl_.glGetError());
        return;
    }
    const uint64_t mark = marked_ + 1u;
    if (count_ == most_pending) {
        Pending &newest = pending_[(first_ + count_ - 1u) % most_pending];
        drop_sync(newest.sync_);
        newest = Pending{sync, mark};
    } else {
        pending_[(first_ + count_) % most_pending] = Pending{sync, mark};
        count_++;
    }
    __atomic_store_n(&marked_, mark, __ATOMIC_RELEASE);
}

bool GlBatchProgress::settle()
{
    AV_SCOPE("GlBatchProgress::settle");
    while (count_ != 0u && fences()) {
        Pending &oldest = pending_[first_];
        const uint32_t status = gl_.glClientWaitSync(static_cast<GLsync>(oldest.sync_), gl_sync_flush_commands_bit, 0u);
        if (status == gl_timeout_expired) break;
        __atomic_store_n(&reached_, oldest.mark_, __ATOMIC_RELEASE);
        drop_sync(oldest.sync_);
        first_ = (first_ + 1u) % most_pending;
        count_--;
    }
    return count_ != 0u;
}

void GlBatchProgress::release()
{
    AV_SCOPE("GlBatchProgress::release");
    while (count_ != 0u) {
        if (fences()) drop_sync(pending_[first_].sync_);
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
        if (row.sync_ == nullptr || row.mark_ <= previous || row.mark_ > marked_) return false;
        previous = row.mark_;
    }
    return previous == marked_ && (count_ == 0u) == (reached_ == marked_);
}

}
