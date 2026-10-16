#include "executor/sharing/host_shares.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/report_gate.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/owned_fd.hpp"
#include "executor/session_backend.hpp"


namespace av {

HostShares::HostShares(BlockSource &blocks, DriverLog &lines)
    : lines_(lines), reported_(0u), lock_(), shares_(blocks), holders_(blocks), retired_(blocks), index_(blocks)
{
    AV_SCOPE("HostShares::HostShares");
}

HostShares::~HostShares()
{
    AV_SCOPE("HostShares::~HostShares");
    /* Nothing is left to hand the device: whatever was waiting goes with
     * retired_. */
    clear();
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
            const ThreadLockHold locked(lock_);
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
            OwnedFd  descriptor;
            if (!session.export_object(kind, handle, handle_kind, descriptor)) {
                answer = Status::not_supported;
            } else {
                const ThreadLockHold locked(lock_);
                if (known->descriptor() < 0) known->keep_descriptor(handle_kind, descriptor);
            }
        }
        settle(*known);
        return answer;
    }

    uint32_t handle_kind = 0u;
    OwnedFd  descriptor;
    if (!session.export_object(kind, handle, handle_kind, descriptor)) return Status::not_supported;
    const ThreadLockHold locked(lock_);
    Share *const share = make(domain, id, kind, size, ShareOrigin::object);
    /* Shared by another session between the export and this lock. */
    if (share == nullptr) return index_.find(domain, id) != nullptr ? Status::conflict : Status::no_memory;
    share->keep_descriptor(handle_kind, descriptor);
    share->publish_by(&session);
    return Status::ok;
}

Status HostShares::open(uint32_t domain, uint64_t id, uint32_t kind, uint64_t size, SessionBackend &session,
                        uint64_t into, uint64_t &handle)
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
        const ThreadLockHold locked(lock_);
        shape = share->shape();
        if (share->descriptor() < 0) answer = Status::not_found;
        else if (!share->accepts(kind, size)) answer = Status::invalid_parameter;
        else if (share->holder_into(session, into) != nullptr) answer = Status::conflict;
        handle_kind = share->handle_kind();
        fd = share->descriptor();
    }
    /* The descriptor stays the share's, open while it is pinned; the importer
     * keeps a duplicate of its own. */
    uint64_t made = 0u;
    uint64_t detail = 0u;
    if (answer == Status::ok && !session.import_object(kind, handle_kind, fd, size, shape, into, made, detail))
        answer = Status::not_supported;
    if (answer == Status::ok) {
        const ThreadLockHold locked(lock_);
        ShareHolder *holder = nullptr;
        if (holders_.create(holder, session, into, made, detail) == Status::ok) {
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

void HostShares::close(uint32_t domain, uint64_t id, SessionBackend &session, uint64_t handle)
{
    AV_SCOPE("HostShares::close");
    Share *const share = pinned(domain, id);
    if (share == nullptr) return;
    uint64_t detail = 0u;
    bool     held = false;
    {
        const ThreadLockHold locked(lock_);
        ShareHolder *const holder = share->holder_answered(session, handle);
        if (holder != nullptr) {
            held = true;
            detail = holder->detail();
            drop(*share, *holder);
        }
    }
    if (held) session.release_object(share->kind(), handle, detail);
    settle(*share);
}

/* ----------------------------------------------------------- Direct3D */

bool HostShares::describe_image(uint32_t domain, uint64_t id, const SharedImageShape &shape)
{
    AV_SCOPE("HostShares::describe_image");
    const ThreadLockHold locked(lock_);
    Share *const share = index_.find(domain, id);
    if (share == nullptr) return false;
    share->describe_image(shape);
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
            const ThreadLockHold locked(lock_);
            ShareHolder *const holder = share->holder_of(session);
            kind = share->kind();
            handle = holder->handle();
            detail = holder->detail();
            drop(*share, *holder);
        }
        session.release_object(kind, handle, detail);
        settle(*share);
    }
    const ThreadLockHold locked(lock_);
    shares_.for_each([&session](const Share &share) {
        if (share.publisher() == &session) const_cast<Share &>(share).publish_by(nullptr);
    });
}

Share *HostShares::held_by(const SessionBackend &session)
{
    AV_SCOPE("HostShares::held_by");
    const ThreadLockHold locked(lock_);
    Share *const share = shares_.find_if([&session](const Share &held) { return held.holder_of(session) != nullptr; });
    if (share != nullptr) share->pin();
    return share;
}

void HostShares::release(uint32_t domain, uint64_t id)
{
    AV_SCOPE("HostShares::release");
    const ThreadLockHold locked(lock_);
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
    const ThreadLockHold locked(lock_);
    shares_.destroy_if([this](const Share &share) {
        Share &freed = const_cast<Share &>(share);
        while (freed.holders() != nullptr) drop(freed, *freed.holders());
        forget(freed);
        return true;
    });
}

uint32_t HostShares::live() const
{
    AV_SCOPE("HostShares::live");
    const ThreadLockHold locked(lock_);
    return shares_.live();
}

void HostShares::report(StateWriter &writer)
{
    AV_SCOPE("HostShares::report");
    /* A dump must not wait on the worker holding the table. */
    const ReportGate gate(lock_, writer, "shares");
    if (!gate.held()) return;
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
        writer.number("opens", share.direct3d().opens());
        writer.number("writes", share.direct3d().writes());
        writer.close();
    });
    writer.close();
}

bool HostShares::invariants_hold() const
{
    AV_SCOPE("HostShares::invariants_hold");
    const ThreadLockHold locked(lock_);
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
    const ThreadLockHold locked(lock_);
    Share *const share = index_.find(domain, id);
    if (share != nullptr) share->pin();
    return share;
}

void HostShares::settle(Share &share)
{
    AV_SCOPE("HostShares::settle");
    const ThreadLockHold locked(lock_);
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
    share.drop_descriptor();
    share.direct3d().retire_all(retired_);
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

}  // namespace av
