#include "miniport/memory/present_reads.hpp"

#include "core/platform/spin_lock_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "miniport/memory/present_writes.hpp"

namespace av {

PresentReads::PresentReads(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::present_reads), open_(0u), expires_(0u)
{
    AV_SCOPE("PresentReads::PresentReads");
}

bool PresentReads::open()
{
    AV_SCOPE("PresentReads::open");
    platform_.require_irql_at_most(Irql::dispatch, "PresentReads::open");
    SpinLockGuard  guard(lock_);
    const uint64_t now = platform_.interrupt_time();
    const bool     forgot = open_ != 0u && !holding(now);
    if (forgot) open_ = 0u;
    const uint64_t expiry = expiry_ticks > ~uint64_t{0} - now ? ~uint64_t{0} : now + expiry_ticks;
    open_++;
    expires_ = expiry;
    return forgot;
}

void PresentReads::close()
{
    AV_SCOPE("PresentReads::close");
    platform_.require_irql_at_most(Irql::dispatch, "PresentReads::close");
    SpinLockGuard guard(lock_);
    if (open_ != 0u) open_--;
}

bool PresentReads::admit(PresentWrites &writes)
{
    AV_SCOPE("PresentReads::admit");
    platform_.require_irql_at_most(Irql::dispatch, "PresentReads::admit");
    SpinLockGuard guard(lock_);
    if (holding(platform_.interrupt_time())) return false;
    writes.opening();
    return true;
}

bool PresentReads::holding(uint64_t now) const
{
    AV_SCOPE("PresentReads::holding");
    return open_ != 0u && now < expires_;
}

bool PresentReads::invariants_hold()
{
    AV_SCOPE("PresentReads::invariants_hold");
    SpinLockGuard guard(lock_);
    return open_ == 0u || expires_ != 0u;
}

}
