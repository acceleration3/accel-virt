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
 * Locking. One lock guards the shares. It is never held across a backend's
 * export, import or release (those may block on a host driver) nor while
 * taking the Direct3D 11 device's lock: the order is that lock first, this one
 * second, and a share a caller works on outside this lock is pinned. COM
 * references of freed shares are not released here, where the Direct3D 11
 * lock may not be held, but handed to take_retired, which the device drains
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
#include "executor/sharing/share.hpp"
#include "executor/sharing/share_holder.hpp"
#include "executor/sharing/share_index.hpp"
#include "device/shim/posix_lock.hpp"

#include <stdint.h>

struct IUnknown;

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
    /* `session` opens `id` as an object of its own, answered as the handle
     * its API names it by. not_found for an id the domain does not share or
     * one with no descriptor; invalid_parameter for a kind or a size that
     * disagrees with it; conflict when the session already holds it or
     * published it; not_supported when its backend imports nothing of this
     * kind. Nothing is held unless ok. */
    [[nodiscard]] Status open(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, SessionBackend &session,
                              uint64_t &handle);
    /* `session` lets go of what it imported as `id`, released through its
     * backend. Nothing for a share it does not hold. */
    void close(uint32_t domain, uint64_t id, SessionBackend &session);

    /* ------------------------------------------------------- Direct3D */

    /* A Direct3D resource, published under `id` as `session` created it, with
     * a reference of the share's own. conflict for an id the domain already
     * shares, invalid_parameter for id 0 or no resource. */
    [[nodiscard]] Status publish_resource(uint32_t domain, uint64_t id, IUnknown *resource, ShareOrigin origin,
                                          SessionBackend &session);
    /* What a Direct3D session opening `id` gets: the flip copy a compositor
     * reads, else the Direct3D 11 view, else the resource - with a reference
     * of the caller's, which is what holds it. Null for an unknown id. */
    IUnknown *open_resource(uint32_t domain, uint64_t id);
    /* The resource `id` was published as, with a reference of the caller's:
     * for making a view of it or exporting it. */
    IUnknown *resource(uint32_t domain, uint64_t id);
    /* The Direct3D 11 view (or the resource), likewise. */
    IUnknown *view(uint32_t domain, uint64_t id);
    /* `view` kept as what a Direct3D 11 session opens `id` as, with a
     * reference of the share's own. False for an unknown id. */
    bool      keep_view(uint32_t domain, uint64_t id, IUnknown *view);
    /* The flip copy, with a reference of the caller's, or null. */
    IUnknown *shown(uint32_t domain, uint64_t id);
    /* `copy` kept as `id`'s flip copy, with a reference of the share's own. */
    bool      keep_shown(uint32_t domain, uint64_t id, IUnknown *copy);
    /* What `id`'s memory is as an image, as its publisher made it: an
     * importer views the memory as that image. False for an unknown id. */
    bool      describe_image(uint32_t domain, uint64_t id, const SharedImageShape &shape);
    /* One more copy into `id` ran on the host. */
    void      written(uint32_t domain, uint64_t id);
    /* How many have: false for an unknown id. */
    bool      writes(uint32_t domain, uint64_t id, uint64_t &count);

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
    /* The GPU's Direct3D 11 or 12 device was lost and is made again: every
     * share lets go of what was that device's (Share::give_up_direct3d11,
     * give_up_direct3d12) to take_retired, which the Direct3D 11 device
     * drains under its lock before the old device goes. The shares stay the
     * guest's until it lets go; one whose resource went opens nothing, as a
     * shared handle from before a TDR opens nothing on Windows. */
    void     lose_direct3d11();
    void     lose_direct3d12();
    /* `visit(domain, id, resource)` for every share Direct3D 11 published
     * whose resource it still holds, under the table's lock: what the lost
     * device made, read before lose_direct3d11 lets it go, so it can be made
     * again (CarriedShare). The visit must not call back into this table. */
    template <typename Visit>
    void     each_direct3d11(Visit visit);
    /* `resource`, made again on the device made in place of a lost one, kept
     * as what the share Direct3D 11 published as `id` is from now on, with a
     * reference of the share's own: false, nothing kept, for an id no share
     * names any more - the guest let it go meanwhile - or one that holds a
     * resource still. */
    [[nodiscard]] bool restore_resource(uint32_t domain, uint64_t id, IUnknown *resource);
    /* COM references of freed shares, now the caller's to release under the
     * Direct3D 11 device's lock: up to `capacity` of them, the count answered. */
    uint32_t take_retired(IUnknown **objects, uint32_t capacity);
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
    /* `share` out of the index, its descriptor closed and its COM objects
     * retired, ready to destroy. The caller holds the lock. */
    void         forget(Share &share);
    /* `holder` taken off `share` and destroyed. The caller holds the lock. */
    void         drop(Share &share, ShareHolder &holder);
    /* A new share, indexed: null when either cannot be had. */
    Share       *make(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, ShareOrigin origin);
    /* The line written when the live shares reach `live`, a power of two. The
     * caller holds the lock. */
    void         milestone(uint32_t live);
    /* `object` kept for take_retired, or released now when nothing can hold it. */
    void         retire(IUnknown *object);
    /* Every share's objects that `give_up` hands over, retired. */
    template <typename GiveUp>
    void         retire_each(GiveUp give_up);

    struct Retired {
        explicit Retired(IUnknown *object);
        IUnknown *object_;
    };

    DriverLog              &lines_;
    /* The highest count a line was written for. */
    uint32_t                reported_;
    mutable PosixLock       lock_;
    ObjectPool<Share>       shares_;
    ObjectPool<ShareHolder> holders_;
    ObjectPool<Retired>     retired_;
    ShareIndex              index_;

    friend struct HostSharesProbe;
};

template <typename Visit>
void HostShares::each_direct3d11(Visit visit)
{
    AV_SCOPE("HostShares::each_direct3d11");
    const ThreadLockHold locked(lock_);
    shares_.for_each([&visit](const Share &share) {
        if (share.origin() == ShareOrigin::d3d11 && share.resource() != nullptr)
            visit(share.domain(), share.id(), share.resource());
    });
}

}  // namespace av

#endif  // AV_HOST_SHARES_HPP
