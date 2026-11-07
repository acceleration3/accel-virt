#ifndef AV_HOST_SHARES_HPP
#define AV_HOST_SHARES_HPP

#include "core/support/object_pool.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/sharing/d3d_retired.hpp"
#include "executor/sharing/share.hpp"
#include "executor/sharing/share_holder.hpp"
#include "executor/sharing/share_index.hpp"
#include "device/shim/posix_lock.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class DriverLog;
class SessionBackend;
class StateWriter;
struct HostSharesProbe;

class HostShares {
public:
    static constexpr uint32_t first_milestone = 64u;

    HostShares(BlockSource &blocks, DriverLog &lines);
    ~HostShares();
    HostShares(const HostShares &) = delete;
    HostShares &operator=(const HostShares &) = delete;

    [[nodiscard]] Status publish(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, SessionBackend &session,
                                 uint64_t handle);
    [[nodiscard]] Status open(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, SessionBackend &session,
                              uint64_t into, uint64_t &handle);
    void close(uint32_t domain, uint64_t id, SessionBackend &session, uint64_t handle);

    template <typename Act>
    [[nodiscard]] Status publish_direct3d(uint32_t domain, uint64_t id, ShareOrigin origin, SessionBackend &session,
                                          Act act);
    template <typename Act>
    bool                 with_direct3d(uint32_t domain, uint64_t id, Act act);
    template <typename Act>
    void                 each_direct3d(Act act);
    template <typename Act>
    void                 with_retired(Act act);
    bool                 describe_image(uint32_t domain, uint64_t id, const SharedImageShape &shape);

    void     session_closed(SessionBackend &session);
    void     session_leaked(SessionBackend &session);
    void     release(uint32_t domain, uint64_t id);
    void     clear();
    uint32_t live() const;
    void     report(StateWriter &writer);
    bool     invariants_hold() const;

private:
    Share       *pinned(uint32_t domain, uint64_t id);
    Share       *held_by(const SessionBackend &session);
    void         settle(Share &share);
    void         free_if_unheld(Share &share);
    void         forget(Share &share);
    void         drop(Share &share, ShareHolder &holder);
    Share       *make(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, ShareOrigin origin);
    void         milestone(uint32_t live);

    DriverLog              &lines_;
    uint32_t                reported_;
    mutable PosixLock       lock_;
    ObjectPool<Share>       shares_;
    ObjectPool<ShareHolder> holders_;
    D3dRetired              retired_;
    ShareIndex              index_;

    friend struct HostSharesProbe;
};

template <typename Act>
Status HostShares::publish_direct3d(uint32_t domain, uint64_t id, ShareOrigin origin, SessionBackend &session,
                                    Act act)
{
    AV_SCOPE("HostShares::publish_direct3d");
    if (id == 0u) return Status::invalid_parameter;
    const ThreadLockHold locked(lock_);
    if (index_.find(domain, id) != nullptr) return Status::conflict;
    Share *const share = make(domain, id, 0u, 0u, origin);
    if (share == nullptr) return Status::no_memory;
    act(*share);
    share->publish_by(&session);
    return Status::ok;
}

template <typename Act>
bool HostShares::with_direct3d(uint32_t domain, uint64_t id, Act act)
{
    AV_SCOPE("HostShares::with_direct3d");
    const ThreadLockHold locked(lock_);
    Share *const share = index_.find(domain, id);
    if (share == nullptr) return false;
    act(*share, retired_);
    return true;
}

template <typename Act>
void HostShares::each_direct3d(Act act)
{
    AV_SCOPE("HostShares::each_direct3d");
    const ThreadLockHold locked(lock_);
    shares_.for_each([this, &act](const Share &share) { act(const_cast<Share &>(share), retired_); });
}

template <typename Act>
void HostShares::with_retired(Act act)
{
    AV_SCOPE("HostShares::with_retired");
    const ThreadLockHold locked(lock_);
    act(retired_);
}

}

#endif
