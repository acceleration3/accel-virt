/* Every guest shared object on one host GPU, of every API: the host's one
 * record of what an id is, what it is made of and who holds it.
 *
 * A guest's driver mints an id over each shared allocation (feature 053), per
 * guest device - a domain here. Direct3D publishes a resource under it as the
 * resource is created, a session publishes its own object (Vulkan memory, a
 * fence, a Direct3D texture made to be shared) by the handle it names it by,
 * and any session of any API opens it by id: a Direct3D session gets the
 * resource itself, any other imports the descriptor the publisher's backend
 * exported. The guest holds every share until its driver says the object is
 * gone from the whole guest; a session that imported one holds it until it
 * closes it or ends. A share no one holds is freed (Share says why the
 * publisher and a Direct3D opener are not holders).
 *
 * Nothing here is a fixed table: shares, holders and the index grow from the
 * BlockSource, because the guest makes shared objects without bound - DWM
 * keeps every Windows.Graphics.Capture buffer it ever opened.
 *
 * What Direct3D keeps of a share - its COM objects, their retirement - is
 * D3dShares', over the rows and the lock here (with_direct3d and its kin):
 * this class knows shares, descriptors and holders, and no COM object.
 *
 * Locking. One lock guards the shares. It is never held across a backend's
 * export, import or release (those may block on a host driver) nor while
 * taking the Direct3D 11 device's lock: the order is that lock first, this one
 * second, and a share a caller works on outside this lock is pinned. COM
 * references of freed shares are not released here, where the Direct3D 11
 * lock may not be held, but retired (D3dRetired), which the device drains
 * under its lock.
 *
 * Seen in the log: a share that cannot be recorded, and the first time the
 * live shares reach each power of two from 64 up, with what they are - a
 * window making and releasing its surfaces about one is not a line per
 * frame - so a guest that
 * keeps opening and never lets go (DWM keeps its capture buffers) is a line
 * that rises rather than a GPU that slowly fills. */
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
    /* Shares are kept in `blocks`, and what is seen is written to `lines`. */
    static constexpr uint32_t first_milestone = 64u;

    HostShares(BlockSource &blocks, DriverLog &lines);
    ~HostShares();
    HostShares(const HostShares &) = delete;
    HostShares &operator=(const HostShares &) = delete;

    /* -------------------------------------------- objects (feature 053) */

    /* `session` shares the object it names `handle` as `id`: its backend
     * exports a descriptor of it for other APIs to import. The same session
     * publishing the same object again is answered ok, as Windows lets a
     * process share one object twice; a Direct3D resource the session
     * published as it created it is described and exported here. conflict
     * for an id the domain already shares otherwise, not_supported when the
     * backend exports nothing of this kind, no_memory when nothing can be
     * recorded. */
    [[nodiscard]] Status publish(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, SessionBackend &session,
                                 uint64_t handle);
    /* `session` opens `id` as an object of its own, made into `into` (what
     * its API names: a Vulkan device or semaphore, 0 for nothing to choose),
     * answered as the handle its API names it by. A session may hold one
     * share once per `into`, its own publication included: a Vulkan context
     * imports what one of its devices published into another. not_found for
     * an id the domain does not share or one with no descriptor;
     * invalid_parameter for a kind or a size that disagrees with it; conflict
     * when the session already holds it made into `into`; not_supported when
     * its backend imports nothing of this kind into that. Nothing is held
     * unless ok. */
    [[nodiscard]] Status open(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, SessionBackend &session,
                              uint64_t into, uint64_t &handle);
    /* `session` lets go of what it imported as `id` and answered as `handle`,
     * released through its backend. Nothing for a hold it does not have. */
    void close(uint32_t domain, uint64_t id, SessionBackend &session, uint64_t handle);

    /* ------------------------------------------------------- Direct3D */

    /* `act(share)` for a new share of `origin` published under `id` by
     * `session`, under the lock: what Direct3D keeps of it is put there.
     * conflict for an id the domain already shares, invalid_parameter for id
     * 0, no_memory when it cannot be recorded. */
    template <typename Act>
    [[nodiscard]] Status publish_direct3d(uint32_t domain, uint64_t id, ShareOrigin origin, SessionBackend &session,
                                          Act act);
    /* `act(share, retired)` for the share `id` names, under the lock; false,
     * nothing done, for an unknown id. */
    template <typename Act>
    bool                 with_direct3d(uint32_t domain, uint64_t id, Act act);
    /* `act(share, retired)` for every share, under the lock. */
    template <typename Act>
    void                 each_direct3d(Act act);
    /* `act(retired)`: what the shares let go of, under the lock. */
    template <typename Act>
    void                 with_retired(Act act);
    /* What a share is as an image, as its publisher made it: an importer
     * views the memory as that image. False for an unknown id. */
    bool                 describe_image(uint32_t domain, uint64_t id, const SharedImageShape &shape);

    /* ------------------------------------------------------- lifetime */

    /* Every hold `session` had let go of, what it imported released through
     * its backend: the session is ending, and its backend shuts down after.
     * What it published stays the guest's. */
    void     session_closed(SessionBackend &session);
    /* The guest's driver let go of `id` everywhere. 0 is every id of the
     * domain, as a device reset is: what sessions still hold stays theirs but
     * no longer answers to its id, which the guest mints again. */
    void     release(uint32_t domain, uint64_t id);
    /* Every share of every domain gone: the host device is closing. */
    void     clear();
    uint32_t live() const;
    /* Shares by origin, holders, and what is waiting to be retired. */
    void     report(StateWriter &writer);
    bool     invariants_hold() const;

private:
    /* A share the index names, pinned so it outlives the lock; null for an
     * unknown id. The caller unpins it with settle. */
    Share       *pinned(uint32_t domain, uint64_t id);
    /* A share `session` holds, pinned; null for none. */
    Share       *held_by(const SessionBackend &session);
    /* `share` unpinned, and freed if that left it held by no one. */
    void         settle(Share &share);
    /* `share` freed if no one holds it. The caller holds the lock. */
    void         free_if_unheld(Share &share);
    /* `share` out of the index, its descriptor closed and its Direct3D
     * objects retired, ready to destroy. The caller holds the lock. */
    void         forget(Share &share);
    /* `holder` taken off `share` and destroyed. The caller holds the lock. */
    void         drop(Share &share, ShareHolder &holder);
    /* A new share, indexed: null when either cannot be had. */
    Share       *make(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, ShareOrigin origin);
    /* The line written when the live shares reach `live`, a power of two. The
     * caller holds the lock. */
    void         milestone(uint32_t live);

    DriverLog              &lines_;
    /* The highest count a line was written for. */
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

}  // namespace av

#endif  // AV_HOST_SHARES_HPP
