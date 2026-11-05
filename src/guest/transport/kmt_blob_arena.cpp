#include "guest/transport/kmt_blob_arena.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/submission/published_slot_layout.hpp"
#include "proto/wire/guest_bytes.hpp"

namespace av {

KmtBlobArena::KmtBlobArena(GuestConnection &connection, KmtTransport &transport, DriverLog &lines)
    : connection_(connection), transport_(transport), lines_(lines), slots_(), control_(), rings_(), regions_(), slots_taken_(false), idle_since_{}, refusals_logged_(0u), failed_step_(nullptr), failed_status_(Status::ok)
{
    AV_SCOPE("KmtBlobArena::KmtBlobArena");
}

KmtBlobArena::~KmtBlobArena()
{
    AV_SCOPE("KmtBlobArena::~KmtBlobArena");
    static_cast<void>(close());
}

Status KmtBlobArena::open_control()
{
    AV_SCOPE("KmtBlobArena::open_control");
    if (control_.constructed()) return Status::ok;
    static_cast<void>(control_.construct(connection_, transport_, slots_));
    const Status opened = control_.object().open_at(0u, PublishedSlotLayout::control_bytes);
    if (opened != Status::ok) {
        control_.destroy();
        return failed_at("control segment open", opened);
    }
    ArenaSegment &control = control_.object();
    volatile unsigned char *const word = control.view() + PublishedSlotLayout(control.bytes()).error_word();
    for (uint64_t at = 0u; at < PublishedSlotLayout::error_word_bytes; at++) word[at] = 0u;
    if (!transport_.blobs().add_region(control.base(), control.view(), static_cast<size_t>(control.bytes()))) {
        static_cast<void>(control.close());
        control_.destroy();
        return failed_at("control region add", Status::invalid_state);
    }
    slots_taken_ = false;
    return Status::ok;
}

Status KmtBlobArena::open(uint64_t bytes)
{
    AV_SCOPE("KmtBlobArena::open");
    if (lending()) return Status::invalid_state;
    if (bytes > max_bytes) return Status::invalid_parameter;
    failed_step_ = nullptr;
    failed_status_ = Status::ok;
    const Status controlled = open_control();
    if (controlled != Status::ok) return controlled;
    uint64_t size = bytes < default_bytes ? default_bytes : bytes;
    if (size > GuestBytes::slot_bytes) size = GuestBytes::slot_bytes;
    static_cast<void>(rings_[0].construct(connection_, transport_, slots_));
    const Status opened = rings_[0].object().open(size);
    if (opened != Status::ok) {
        rings_[0].destroy();
        return failed_at("ring segment open", opened);
    }
    ArenaSegment &ring = rings_[0].object();
    if (!transport_.blobs().add_segment(ring.base(), ring.view(), static_cast<size_t>(ring.bytes()))) {
        static_cast<void>(ring.close());
        rings_[0].destroy();
        return failed_at("ring region add", Status::invalid_state);
    }
    transport_.attach_arena(this);
    return Status::ok;
}

Status KmtBlobArena::settle(bool abandoning)
{
    AV_SCOPE("KmtBlobArena::settle");
    transport_.attach_arena(nullptr);
    transport_.blobs().lend(nullptr, 0u);
    refusals_logged_ = 0u;
    Status result = Status::ok;
    auto   note = [&result](Status status) {
        if (result == Status::ok) result = status;
    };
    for (ObjectStorage<ArenaSegment> &storage : regions_) {
        if (!storage.constructed()) continue;
        note(abandoning ? storage.object().abandon() : storage.object().close());
        storage.destroy();
    }
    for (ObjectStorage<ArenaSegment> &storage : rings_) {
        if (!storage.constructed()) continue;
        note(abandoning ? storage.object().abandon() : storage.object().close());
        storage.destroy();
    }
    if (control_.constructed()) {
        note(abandoning ? control_.object().abandon() : control_.object().close());
        control_.destroy();
    }
    slots_taken_ = false;
    return result;
}

Status KmtBlobArena::close()
{
    AV_SCOPE("KmtBlobArena::close");
    return settle(false);
}

Status KmtBlobArena::abandon()
{
    AV_SCOPE("KmtBlobArena::abandon");
    return settle(true);
}

Status KmtBlobArena::grow(uint64_t need)
{
    AV_SCOPE("KmtBlobArena::grow");
    const uint64_t total = transport_.blobs().ring_bytes();
    const uint64_t doubled = total < GuestBytes::slot_bytes ? total : GuestBytes::slot_bytes;
    const uint64_t page = ArenaSegment::page_bytes;
    Status         grown = lending() ? Status::no_space : Status::invalid_state;
    if (lending() && need <= GuestBytes::slot_bytes) {
        uint64_t want = doubled > need ? doubled : need;
        want = (want + page - 1u) / page * page;
        if (want != 0u && want >= need && want <= GuestBytes::slot_bytes && total + want <= max_bytes)
            grown = add_ring(want);
    }
    if (grown == Status::ok) refusals_logged_ = 0u;
    if (grown != Status::ok && first_refusal(grown))
        LogLine(lines_)
            .text("the blob arena did not grow by ")
            .number(need)
            .text(" bytes over ")
            .number(total)
            .text(", status ")
            .number(static_cast<uint32_t>(grown))
            .write();
    return grown;
}

const char *KmtBlobArena::failed_step() const
{
    AV_SCOPE("KmtBlobArena::failed_step");
    return failed_step_;
}

Status KmtBlobArena::failed_status() const
{
    AV_SCOPE("KmtBlobArena::failed_status");
    return failed_status_;
}

Status KmtBlobArena::failed_at(const char *step, Status status)
{
    AV_SCOPE("KmtBlobArena::failed_at");
    failed_step_ = step;
    failed_status_ = status;
    return status;
}

bool KmtBlobArena::first_refusal(Status refused)
{
    AV_SCOPE("KmtBlobArena::first_refusal");
    const uint32_t bit = uint32_t{1} << static_cast<unsigned>(refused);
    if ((refusals_logged_ & bit) != 0u) return false;
    refusals_logged_ |= bit;
    return true;
}

Status KmtBlobArena::add_ring(uint64_t want)
{
    AV_SCOPE("KmtBlobArena::add_ring");
    for (ObjectStorage<ArenaSegment> &storage : rings_) {
        if (storage.constructed()) continue;
        static_cast<void>(storage.construct(connection_, transport_, slots_));
        ArenaSegment &made = storage.object();
        const Status  opened = made.open(want);
        if (opened != Status::ok) {
            storage.destroy();
            return opened;
        }
        if (!transport_.blobs().add_segment(made.base(), made.view(), static_cast<size_t>(made.bytes()))) {
            static_cast<void>(made.close());
            storage.destroy();
            return Status::no_space;
        }
        return Status::ok;
    }
    return Status::no_space;
}

Status KmtBlobArena::release_idle(uint64_t now)
{
    AV_SCOPE("KmtBlobArena::release_idle");
    Status result = Status::ok;
    for (uint32_t index = 1u; index < ClientBlobArena::max_segments; index++) {
        ObjectStorage<ArenaSegment> &storage = rings_[index];
        if (!storage.constructed()) {
            idle_since_[index] = 0u;
            continue;
        }
        if (!transport_.blobs().idle_at(storage.object().base())) {
            idle_since_[index] = 0u;
            continue;
        }
        if (idle_since_[index] == 0u) idle_since_[index] = now;
        if (now - idle_since_[index] < idle_release_ticks) continue;
        static_cast<void>(transport_.blobs().remove_segment(storage.object().base()));
        const Status closed = storage.object().close();
        if (result == Status::ok) result = closed;
        storage.destroy();
        idle_since_[index] = 0u;
    }
    return result;
}

Status KmtBlobArena::open_region(uint64_t bytes, ArenaSegment *&region)
{
    AV_SCOPE("KmtBlobArena::open_region");
    region = nullptr;
    if (!lending() || bytes == 0u || bytes > max_bytes) return Status::invalid_parameter;
    for (ObjectStorage<ArenaSegment> &storage : regions_) {
        if (storage.constructed()) continue;
        static_cast<void>(storage.construct(connection_, transport_, slots_));
        ArenaSegment &made = storage.object();
        const Status  opened = made.open(bytes);
        if (opened != Status::ok) {
            storage.destroy();
            return opened;
        }
        if (!transport_.blobs().add_region(made.base(), made.view(), static_cast<size_t>(made.bytes()))) {
            static_cast<void>(made.close());
            storage.destroy();
            return Status::no_space;
        }
        region = &made;
        return Status::ok;
    }
    return Status::no_space;
}

Status KmtBlobArena::close_region(uint64_t base)
{
    AV_SCOPE("KmtBlobArena::close_region");
    for (ObjectStorage<ArenaSegment> &storage : regions_) {
        if (!storage.constructed() || storage.object().base() != base) continue;
        static_cast<void>(transport_.blobs().remove_region(base));
        const Status closed = storage.object().close();
        storage.destroy();
        return closed;
    }
    return Status::not_found;
}

bool KmtBlobArena::take_query_slots(uint32_t count, uint8_t *&view, uint64_t &offset)
{
    AV_SCOPE("KmtBlobArena::take_query_slots");
    if (!control_.constructed() || slots_taken_) return false;
    const ArenaSegment       &control = control_.object();
    const PublishedSlotLayout tail(control.bytes());
    const uint64_t            at = tail.slots_at(count);
    if (at == 0u) return false;
    slots_taken_ = true;
    view = control.view() + at;
    offset = control.base() + at;
    return true;
}

bool KmtBlobArena::lending() const
{
    AV_SCOPE("KmtBlobArena::lending");
    return rings_[0].constructed();
}

uint64_t KmtBlobArena::bytes() const
{
    AV_SCOPE("KmtBlobArena::bytes");
    uint64_t total = control_.constructed() ? control_.object().bytes() : 0u;
    for (const ObjectStorage<ArenaSegment> &storage : rings_)
        if (storage.constructed()) total += storage.object().bytes();
    for (const ObjectStorage<ArenaSegment> &storage : regions_)
        if (storage.constructed()) total += storage.object().bytes();
    return total;
}

uint64_t KmtBlobArena::host_errors() const
{
    AV_SCOPE("KmtBlobArena::host_errors");
    if (!control_.constructed()) return 0u;
    const ArenaSegment &control = control_.object();
    const auto *const   word = reinterpret_cast<const volatile uint64_t *>(
        control.view() + PublishedSlotLayout(control.bytes()).error_word());
    return __atomic_load_n(word, __ATOMIC_ACQUIRE);
}

bool KmtBlobArena::invariants_hold() const
{
    AV_SCOPE("KmtBlobArena::invariants_hold");
    if (!control_.constructed()) {
        for (const ObjectStorage<ArenaSegment> &storage : rings_)
            if (storage.constructed()) return false;
        return !transport_.blobs().ready();
    }
    if (!control_.object().invariants_hold() || control_.object().base() != 0u) return false;
    uint32_t rings = 0u;
    for (const ObjectStorage<ArenaSegment> &storage : rings_) {
        if (!storage.constructed()) continue;
        if (!storage.object().invariants_hold() || storage.object().bytes() > GuestBytes::slot_bytes) return false;
        rings++;
    }
    for (const ObjectStorage<ArenaSegment> &storage : regions_)
        if (storage.constructed() && !storage.object().invariants_hold()) return false;
    return rings == transport_.blobs().segments();
}

}
