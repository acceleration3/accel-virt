#include "executor/sharing/host_shares.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/session_backend.hpp"
#include "executor/shim/d3d_headers.hpp"

#include <unistd.h>

namespace av {

/* The table's lock for one scope. */
class HostShares::Lock {
public:
    explicit Lock(pthread_mutex_t &mutex);
    ~Lock();
    Lock(const Lock &) = delete;
    Lock &operator=(const Lock &) = delete;

private:
    pthread_mutex_t &mutex_;
};

HostShares::Lock::Lock(pthread_mutex_t &mutex) : mutex_(mutex)
{
    AV_SCOPE("HostShares::Lock::Lock");
    pthread_mutex_lock(&mutex_);
}

HostShares::Lock::~Lock()
{
    AV_SCOPE("HostShares::Lock::~Lock");
    pthread_mutex_unlock(&mutex_);
}

HostShares::Retired::Retired(IUnknown *object) : object_(object)
{
    AV_SCOPE("HostShares::Retired::Retired");
}

HostShares::HostShares(BlockSource &blocks, DriverLog &lines)
    : lines_(lines), reported_(0u), mutex_(), shares_(blocks), holders_(blocks), retired_(blocks), index_(blocks)
{
    AV_SCOPE("HostShares::HostShares");
    pthread_mutex_init(&mutex_, nullptr);
}

HostShares::~HostShares()
{
    AV_SCOPE("HostShares::~HostShares");
    clear();
    /* Nothing is left to hand the device: whatever was waiting goes now. */
    retired_.destroy_if([](const Retired &retired) {
        retired.object_->Release();
        return true;
    });
    pthread_mutex_destroy(&mutex_);
}

/* ------------------------------------------------------------ objects */

Status HostShares::publish(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, SessionBackend &session,
                           uint64_t handle)
{
    AV_SCOPE("HostShares::publish");
    if (id == 0u) return Status::invalid_parameter;
    Share *const known = pinned(domain, id);
    if (known != nullptr) {
        /* Shared already: by this session as the same object (again, or a
         * Direct3D resource it made shared and now hands to other APIs), or by
         * somebody else, which a hostile guest would drive at the worst time. */
        bool mine = false;
        bool exported = false;
        {
            const Lock locked(mutex_);
            const bool publisher = known->publisher() == &session;
            /* A Direct3D resource is published before anyone says what kind of
             * object it is to other APIs: its first publish by handle does. */
            if (publisher && known->origin() != ShareOrigin::object && known->descriptor() < 0)
                known->describe(kind, size);
            mine = publisher && known->accepts(kind, size);
            exported = known->descriptor() >= 0;
        }
        Status answer = mine ? Status::ok : Status::conflict;
        if (mine && !exported) {
            uint32_t handle_kind = 0u;
            int      fd = -1;
            if (!session.export_object(kind, handle, handle_kind, fd)) {
                answer = Status::not_supported;
            } else {
                const Lock locked(mutex_);
                if (known->descriptor() < 0) known->keep_descriptor(handle_kind, fd);
                else ::close(fd);
            }
        }
        settle(*known);
        return answer;
    }

    uint32_t handle_kind = 0u;
    int      fd = -1;
    if (!session.export_object(kind, handle, handle_kind, fd)) return Status::not_supported;
    const Lock   locked(mutex_);
    Share *const share = make(domain, id, kind, size, ShareOrigin::object);
    if (share == nullptr) {
        ::close(fd);
        /* Shared by another session between the export and this lock. */
        return index_.find(domain, id) != nullptr ? Status::conflict : Status::no_memory;
    }
    share->keep_descriptor(handle_kind, fd);
    share->publish_by(&session);
    return Status::ok;
}

Status HostShares::open(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, SessionBackend &session,
                        uint64_t &handle)
{
    AV_SCOPE("HostShares::open");
    handle = 0u;
    Share *const share = pinned(domain, id);
    if (share == nullptr) return Status::not_found;
    Status   answer = Status::ok;
    uint32_t         handle_kind = 0u;
    int              fd = -1;
    SharedImageShape shape;
    {
        const Lock locked(mutex_);
        shape = share->shape();
        if (share->descriptor() < 0) answer = Status::not_found;
        else if (!share->accepts(kind, size)) answer = Status::invalid_parameter;
        else if (share->holder_of(session) != nullptr || share->publisher() == &session) answer = Status::conflict;
        handle_kind = share->handle_kind();
        fd = share->descriptor();
    }
    /* The descriptor stays the share's, open while it is pinned; the importer
     * keeps a duplicate of its own. */
    uint64_t made = 0u;
    uint64_t detail = 0u;
    if (answer == Status::ok && !session.import_object(kind, handle_kind, fd, size, shape, made, detail))
        answer = Status::not_supported;
    if (answer == Status::ok) {
        const Lock   locked(mutex_);
        ShareHolder *holder = nullptr;
        if (holders_.create(holder, session, made, detail) == Status::ok) {
            share->hold(*holder);
            handle = made;
        } else {
            answer = Status::no_memory;
            lines_.note_hex("a share cannot be opened, out of memory: id", id, 16u);
        }
    }
    if (answer == Status::no_memory) session.release_object(kind, made, detail);
    settle(*share);
    return answer;
}

void HostShares::close(uint32_t domain, uint64_t id, SessionBackend &session)
{
    AV_SCOPE("HostShares::close");
    Share *const share = pinned(domain, id);
    if (share == nullptr) return;
    uint64_t handle = 0u;
    uint64_t detail = 0u;
    bool     held = false;
    {
        const Lock         locked(mutex_);
        ShareHolder *const holder = share->holder_of(session);
        if (holder != nullptr) {
            held = true;
            handle = holder->handle();
            detail = holder->detail();
            drop(*share, *holder);
        }
    }
    if (held) session.release_object(share->kind(), handle, detail);
    settle(*share);
}

/* ----------------------------------------------------------- Direct3D */

Status HostShares::publish_resource(uint32_t domain, uint64_t id, IUnknown *resource, ShareOrigin origin,
                                    SessionBackend &session)
{
    AV_SCOPE("HostShares::publish_resource");
    if (id == 0u || resource == nullptr) return Status::invalid_parameter;
    const Lock locked(mutex_);
    if (index_.find(domain, id) != nullptr) return Status::conflict;
    Share *const share = make(domain, id, 0u, 0u, origin);
    if (share == nullptr) return Status::no_memory;
    resource->AddRef();
    share->keep_resource(resource);
    share->publish_by(&session);
    return Status::ok;
}

IUnknown *HostShares::open_resource(uint32_t domain, uint64_t id)
{
    AV_SCOPE("HostShares::open_resource");
    const Lock   locked(mutex_);
    Share *const share = index_.find(domain, id);
    if (share == nullptr) return nullptr;
    IUnknown *const opened = share->shown() != nullptr ? share->shown() : share->view();
    if (opened == nullptr) return nullptr;
    share->opened();
    opened->AddRef();
    return opened;
}

IUnknown *HostShares::resource(uint32_t domain, uint64_t id)
{
    AV_SCOPE("HostShares::resource");
    const Lock         locked(mutex_);
    const Share *const share = index_.find(domain, id);
    IUnknown *const    found = share != nullptr ? share->resource() : nullptr;
    if (found != nullptr) found->AddRef();
    return found;
}

IUnknown *HostShares::view(uint32_t domain, uint64_t id)
{
    AV_SCOPE("HostShares::view");
    const Lock         locked(mutex_);
    const Share *const share = index_.find(domain, id);
    IUnknown *const    found = share != nullptr ? share->view() : nullptr;
    if (found != nullptr) found->AddRef();
    return found;
}

bool HostShares::keep_view(uint32_t domain, uint64_t id, IUnknown *view)
{
    AV_SCOPE("HostShares::keep_view");
    if (view == nullptr) return false;
    const Lock   locked(mutex_);
    Share *const share = index_.find(domain, id);
    if (share == nullptr) return false;
    /* Only a view of its own is replaced: the resource itself stays. */
    if (share->view() != share->resource()) retire(share->view());
    view->AddRef();
    share->keep_view(view);
    return true;
}

IUnknown *HostShares::shown(uint32_t domain, uint64_t id)
{
    AV_SCOPE("HostShares::shown");
    const Lock         locked(mutex_);
    const Share *const share = index_.find(domain, id);
    IUnknown *const    found = share != nullptr ? share->shown() : nullptr;
    if (found != nullptr) found->AddRef();
    return found;
}

bool HostShares::keep_shown(uint32_t domain, uint64_t id, IUnknown *copy)
{
    AV_SCOPE("HostShares::keep_shown");
    if (copy == nullptr) return false;
    const Lock   locked(mutex_);
    Share *const share = index_.find(domain, id);
    if (share == nullptr) return false;
    if (share->shown() != nullptr) retire(share->shown());
    copy->AddRef();
    share->keep_shown(copy);
    return true;
}

bool HostShares::describe_image(uint32_t domain, uint64_t id, const SharedImageShape &shape)
{
    AV_SCOPE("HostShares::describe_image");
    const Lock   locked(mutex_);
    Share *const share = index_.find(domain, id);
    if (share == nullptr) return false;
    share->describe_image(shape);
    return true;
}

void HostShares::written(uint32_t domain, uint64_t id)
{
    AV_SCOPE("HostShares::written");
    const Lock   locked(mutex_);
    Share *const share = index_.find(domain, id);
    if (share != nullptr) share->written();
}

bool HostShares::writes(uint32_t domain, uint64_t id, uint64_t &count)
{
    AV_SCOPE("HostShares::writes");
    count = 0u;
    const Lock         locked(mutex_);
    const Share *const share = index_.find(domain, id);
    if (share == nullptr) return false;
    count = share->writes();
    return true;
}

/* ----------------------------------------------------------- lifetime */

/* What it imported is released through it one hold at a time, outside the
 * lock as a close is: a backend's release may wait on its host driver. */
void HostShares::session_closed(SessionBackend &session)
{
    AV_SCOPE("HostShares::session_closed");
    for (Share *share = held_by(session); share != nullptr; share = held_by(session)) {
        uint32_t kind = 0u;
        uint64_t handle = 0u;
        uint64_t detail = 0u;
        {
            const Lock         locked(mutex_);
            ShareHolder *const holder = share->holder_of(session);
            kind = share->kind();
            handle = holder->handle();
            detail = holder->detail();
            drop(*share, *holder);
        }
        session.release_object(kind, handle, detail);
        settle(*share);
    }
    const Lock locked(mutex_);
    shares_.for_each([&session](const Share &share) {
        if (share.publisher() == &session) const_cast<Share &>(share).publish_by(nullptr);
    });
}

Share *HostShares::held_by(const SessionBackend &session)
{
    AV_SCOPE("HostShares::held_by");
    const Lock   locked(mutex_);
    Share *const share = shares_.find_if([&session](const Share &held) { return held.holder_of(session) != nullptr; });
    if (share != nullptr) share->pin();
    return share;
}

void HostShares::release(uint32_t domain, uint64_t id)
{
    AV_SCOPE("HostShares::release");
    const Lock locked(mutex_);
    if (id != 0u) {
        Share *const share = index_.find(domain, id);
        if (share == nullptr) return;
        share->let_go_by_guest();
        free_if_unheld(*share);
        return;
    }
    shares_.for_each([this, domain](const Share &visited) {
        if (visited.domain() != domain) return;
        Share &share = const_cast<Share &>(visited);
        share.let_go_by_guest();
        if (share.unheld() || !share.indexed()) return;
        index_.remove(domain, share.id());
        share.unindex();
    });
    shares_.destroy_if([this, domain](const Share &share) {
        if (share.domain() != domain || !share.unheld()) return false;
        forget(const_cast<Share &>(share));
        return true;
    });
}

void HostShares::clear()
{
    AV_SCOPE("HostShares::clear");
    const Lock locked(mutex_);
    shares_.destroy_if([this](const Share &share) {
        Share &freed = const_cast<Share &>(share);
        while (freed.holders() != nullptr) drop(freed, *freed.holders());
        forget(freed);
        return true;
    });
}

template <typename GiveUp>
void HostShares::retire_each(GiveUp give_up)
{
    AV_SCOPE("HostShares::retire_each");
    const Lock locked(mutex_);
    shares_.for_each([this, &give_up](const Share &visited) {
        IUnknown *objects[3];
        const uint32_t given = give_up(const_cast<Share &>(visited), objects);
        for (uint32_t index = 0u; index < given; index++) retire(objects[index]);
    });
}

void HostShares::lose_direct3d11()
{
    AV_SCOPE("HostShares::lose_direct3d11");
    retire_each([](Share &share, IUnknown **objects) { return share.give_up_direct3d11(objects); });
}

bool HostShares::restore_resource(uint32_t domain, uint64_t id, IUnknown *resource)
{
    AV_SCOPE("HostShares::restore_resource");
    if (resource == nullptr) return false;
    const Lock   locked(mutex_);
    Share *const share = index_.find(domain, id);
    if (share == nullptr || share->origin() != ShareOrigin::d3d11 || share->resource() != nullptr) return false;
    resource->AddRef();
    share->keep_resource(resource);
    return true;
}

void HostShares::lose_direct3d12()
{
    AV_SCOPE("HostShares::lose_direct3d12");
    retire_each([](Share &share, IUnknown **objects) { return share.give_up_direct3d12(objects); });
}

uint32_t HostShares::take_retired(IUnknown **objects, uint32_t capacity)
{
    AV_SCOPE("HostShares::take_retired");
    const Lock locked(mutex_);
    uint32_t   taken = 0u;
    retired_.destroy_if([objects, capacity, &taken](const Retired &retired) {
        if (taken == capacity) return false;
        objects[taken++] = retired.object_;
        return true;
    });
    return taken;
}

uint32_t HostShares::live() const
{
    AV_SCOPE("HostShares::live");
    const Lock locked(mutex_);
    return shares_.live();
}

void HostShares::report(StateWriter &writer)
{
    AV_SCOPE("HostShares::report");
    writer.open("shares", 0u);
    if (pthread_mutex_trylock(&mutex_) != 0) {
        /* A dump must not wait on the worker holding the table. */
        writer.flag("busy", true);
        writer.close();
        return;
    }
    uint32_t by_origin[3] = {0u, 0u, 0u};
    uint32_t released = 0u;
    uint32_t unindexed = 0u;
    shares_.for_each([&by_origin, &released, &unindexed](const Share &share) {
        by_origin[static_cast<uint32_t>(share.origin())]++;
        if (!share.guest_holds()) released++;
        if (!share.indexed()) unindexed++;
    });
    writer.number("live", shares_.live());
    writer.number("objects", by_origin[static_cast<uint32_t>(ShareOrigin::object)]);
    writer.number("d3d11", by_origin[static_cast<uint32_t>(ShareOrigin::d3d11)]);
    writer.number("d3d12", by_origin[static_cast<uint32_t>(ShareOrigin::d3d12)]);
    writer.number("released_by_guest", released);
    writer.number("unindexed", unindexed);
    writer.number("holders", holders_.live());
    writer.number("retiring", retired_.live());
    shares_.for_each([&writer](const Share &share) {
        writer.open("share", share.id());
        writer.number("domain", share.domain());
        writer.number("origin", static_cast<uint32_t>(share.origin()));
        writer.number("kind", share.kind());
        writer.number("size", share.size());
        writer.flag("guest_holds", share.guest_holds());
        writer.flag("published", share.publisher() != nullptr);
        writer.flag("exported", share.descriptor() >= 0);
        uint32_t holders = 0u;
        for (const ShareHolder *holder = share.holders(); holder != nullptr; holder = holder->next()) holders++;
        writer.number("holders", holders);
        writer.number("opens", share.opens());
        writer.number("writes", share.writes());
        writer.close();
    });
    pthread_mutex_unlock(&mutex_);
    writer.close();
}

bool HostShares::invariants_hold() const
{
    AV_SCOPE("HostShares::invariants_hold");
    const Lock locked(mutex_);
    uint32_t   holders = 0u;
    uint32_t   indexed = 0u;
    uint32_t   unfound = 0u;
    shares_.for_each([this, &holders, &indexed, &unfound](const Share &share) {
        if (share.indexed()) {
            indexed++;
            if (index_.find(share.domain(), share.id()) != &share) unfound++;
        }
        for (const ShareHolder *holder = share.holders(); holder != nullptr; holder = holder->next()) holders++;
    });
    return unfound == 0u && index_.count() == indexed && holders == holders_.live();
}

/* ------------------------------------------------------------ private */

Share *HostShares::pinned(uint32_t domain, uint64_t id)
{
    AV_SCOPE("HostShares::pinned");
    const Lock   locked(mutex_);
    Share *const share = index_.find(domain, id);
    if (share != nullptr) share->pin();
    return share;
}

void HostShares::settle(Share &share)
{
    AV_SCOPE("HostShares::settle");
    const Lock locked(mutex_);
    share.unpin();
    free_if_unheld(share);
}

void HostShares::free_if_unheld(Share &share)
{
    AV_SCOPE("HostShares::free_if_unheld");
    if (!share.unheld()) return;
    forget(share);
    static_cast<void>(shares_.destroy(&share));
}

void HostShares::forget(Share &share)
{
    AV_SCOPE("HostShares::forget");
    if (share.indexed()) index_.remove(share.domain(), share.id());
    share.unindex();
    const int fd = share.take_descriptor();
    if (fd >= 0) ::close(fd);
    IUnknown *objects[3];
    for (uint32_t index = 0u, given = share.give_up_objects(objects); index < given; index++) retire(objects[index]);
}

void HostShares::drop(Share &share, ShareHolder &holder)
{
    AV_SCOPE("HostShares::drop");
    share.unhold(holder);
    static_cast<void>(holders_.destroy(&holder));
}

Share *HostShares::make(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, ShareOrigin origin)
{
    AV_SCOPE("HostShares::make");
    if (index_.find(domain, id) != nullptr) return nullptr;
    Share *share = nullptr;
    if (shares_.create(share, domain, id, kind, size, origin) != Status::ok) {
        lines_.note_hex("a share cannot be recorded, out of memory: id", id, 16u);
        return nullptr;
    }
    if (index_.insert(domain, id, share) != Status::ok) {
        static_cast<void>(shares_.destroy(share));
        lines_.note_hex("a share cannot be indexed, out of memory: id", id, 16u);
        return nullptr;
    }
    const uint32_t live = shares_.live();
    if (live >= first_milestone && live > reported_ && (live & (live - 1u)) == 0u) milestone(live);
    return share;
}

void HostShares::milestone(uint32_t live)
{
    AV_SCOPE("HostShares::milestone");
    reported_ = live;
    uint32_t by_origin[3] = {0u, 0u, 0u};
    uint32_t released = 0u;
    shares_.for_each([&by_origin, &released](const Share &share) {
        by_origin[static_cast<uint32_t>(share.origin())]++;
        if (!share.guest_holds()) released++;
    });
    LogLine(lines_)
        .text("shares live: ")
        .number(live)
        .text(" (")
        .number(by_origin[static_cast<uint32_t>(ShareOrigin::object)])
        .text(" objects, ")
        .number(by_origin[static_cast<uint32_t>(ShareOrigin::d3d11)])
        .text(" d3d11, ")
        .number(by_origin[static_cast<uint32_t>(ShareOrigin::d3d12)])
        .text(" d3d12, ")
        .number(released)
        .text(" released by the guest, ")
        .number(holders_.live())
        .text(" holders)")
        .write();
}

void HostShares::retire(IUnknown *object)
{
    AV_SCOPE("HostShares::retire");
    Retired *retired = nullptr;
    if (retired_.create(retired, object) != Status::ok) object->Release();
}

}  // namespace av
