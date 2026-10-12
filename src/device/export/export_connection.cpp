#include "device/export/export_connection.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/entry_points.hpp"
#include "device/display/display_clock.hpp"
#include "device/export/exported_gpu.hpp"
#include "device/host/host.hpp"
#include "proto/export/export_answer.hpp"
#include "proto/export/export_request.hpp"

namespace av {

ExportConnection::ExportConnection(Host &host, const ExportedGpu &gpu, DisplayClock &clock)
    : host_(host), gpu_(gpu), clock_(clock), lock_(nullptr), thread_(nullptr), descriptor_(-1), ended_(false), surfaces_(nullptr),
      room_(0u), held_(0u)
{
    AV_SCOPE("ExportConnection::ExportConnection");
}

ExportConnection::~ExportConnection()
{
    AV_SCOPE("ExportConnection::~ExportConnection");
    close();
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
    if (surfaces_ != nullptr) host_.release(surfaces_);
}

Status ExportConnection::open(int descriptor)
{
    AV_SCOPE("ExportConnection::open");
    host_.require_main_loop("ExportConnection::open");
    if (thread_ != nullptr) return Status::invalid_state;
    if (lock_ == nullptr && host_.create_mutex(lock_) != Status::ok) {
        lock_ = nullptr;
        return Status::no_memory;
    }
    descriptor_ = descriptor;
    ended_ = false;
    held_ = 0u;
    if (host_.start_thread(&AvExportConnectionServe, this, thread_) == Status::ok) return Status::ok;
    thread_ = nullptr;
    descriptor_ = -1;
    return Status::no_memory;
}

/* One byte past a request, so a datagram too long for one still arrives too
 * long, and decodes to nothing. */
void ExportConnection::serve()
{
    AV_SCOPE("ExportConnection::serve");
    uint8_t bytes[ExportRequest::encoded_size + 1u] = {};
    bool    answering = true;
    while (answering) {
        size_t length = 0u;
        if (host_.receive_export(descriptor_, bytes, sizeof bytes, length) != Status::ok) break;
        host_.lock_mutex(lock_);
        answering = answer(bytes, length < sizeof bytes ? length : sizeof bytes);
        host_.unlock_mutex(lock_);
    }
    /* The consumer learns it is done with at once - an answer it would not
     * take ends it too - while the descriptor waits for the main loop. */
    host_.shutdown_export(descriptor_);
    host_.lock_mutex(lock_);
    held_ = 0u;
    ended_ = true;
    host_.unlock_mutex(lock_);
}

bool ExportConnection::answer(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ExportConnection::answer");
    ExportRequest request;
    const Status  decoded = request.decode(bytes, length);
    if (decoded != Status::ok) return send(ExportAnswer(decoded), -1);
    int                descriptor = -1;
    const ExportAnswer reply = respond(request, descriptor);
    const bool         sent = send(reply, descriptor);
    if (descriptor >= 0) host_.close_export(descriptor);
    return sent;
}

/* Sync and release name a surface this consumer exported: what it holds is
 * what it is told of when the surface goes. A release of one it does not hold
 * is answered ok all the same - releasing is idempotent. A timing the clock
 * cannot take yet - the device not realized - is unavailable. */
ExportAnswer ExportConnection::respond(const ExportRequest &request, int &descriptor)
{
    AV_SCOPE("ExportConnection::respond");
    descriptor = -1;
    const uint64_t surface = request.surface();
    switch (request.operation()) {
    case ExportOperation::hello: return gpu_.greet(request);
    case ExportOperation::release:
        drop(surface);
        return ExportAnswer(request, Status::ok);
    case ExportOperation::sync:
        if (!holds(surface)) return ExportAnswer(request, Status::not_found);
        return gpu_.sync(request, descriptor);
    case ExportOperation::timing:
        return ExportAnswer(request, clock_.align(request.refresh_millihz(), request.blank()) == Status::ok
                                         ? Status::ok
                                         : Status::invalid_state);
    default: break;
    }
    const bool held = holds(surface);
    /* Room first: a surface exported with nowhere to record it is one the
     * consumer would never be told is gone. */
    if (!held && !room_for_one()) return ExportAnswer(request, Status::no_space);
    const ExportAnswer exported = gpu_.export_surface(request, descriptor);
    if (exported.status() == Status::ok && !held) surfaces_[held_++] = surface;
    return exported;
}

/* Every answer here is built from a decoded request or a revocation, and
 * every body from what ExportedGpu checked, so each encodes into the 64 bytes
 * it is given. A consumer that does not read its answers is one that has
 * gone: the send does not wait. */
bool ExportConnection::send(const ExportAnswer &answer, int descriptor)
{
    AV_SCOPE("ExportConnection::send");
    uint8_t bytes[ExportAnswer::encoded_size] = {};
    static_cast<void>(answer.encode(bytes, sizeof bytes));
    return host_.send_export(descriptor_, bytes, sizeof bytes, descriptor) == Status::ok;
}

void ExportConnection::revoke(uint64_t surface, ExportRevocation reason)
{
    AV_SCOPE("ExportConnection::revoke");
    if (thread_ == nullptr) return;
    host_.lock_mutex(lock_);
    const bool told = !ended_ && (surface == 0u || holds(surface));
    if (surface == 0u) held_ = 0u;
    else drop(surface);
    /* A consumer that cannot be told is one that reads nothing: its next
     * receive ends it. */
    if (told && !send(ExportAnswer(reason, surface), -1)) host_.shutdown_export(descriptor_);
    host_.unlock_mutex(lock_);
}

bool ExportConnection::serving() const
{
    AV_SCOPE("ExportConnection::serving");
    return thread_ != nullptr;
}

bool ExportConnection::ended() const
{
    AV_SCOPE("ExportConnection::ended");
    if (thread_ == nullptr) return false;
    host_.lock_mutex(lock_);
    const bool gone = ended_;
    host_.unlock_mutex(lock_);
    return gone;
}

void ExportConnection::close()
{
    AV_SCOPE("ExportConnection::close");
    if (thread_ == nullptr) return;
    host_.shutdown_export(descriptor_);
    host_.join_thread(thread_);
    thread_ = nullptr;
    host_.close_export(descriptor_);
    descriptor_ = -1;
    held_ = 0u;
    ended_ = false;
}

uint32_t ExportConnection::held() const
{
    AV_SCOPE("ExportConnection::held");
    return held_;
}

bool ExportConnection::holds(uint64_t surface) const
{
    AV_SCOPE("ExportConnection::holds");
    for (uint32_t index = 0u; index < held_; index++)
        if (surfaces_[index] == surface) return true;
    return false;
}

bool ExportConnection::room_for_one()
{
    AV_SCOPE("ExportConnection::room_for_one");
    if (held_ < room_) return true;
    const uint32_t  grown = room_ == 0u ? first_room : room_ * 2u;
    uint64_t *const list = static_cast<uint64_t *>(host_.allocate(grown * sizeof(uint64_t)));
    if (list == nullptr) return false;
    for (uint32_t index = 0u; index < held_; index++) list[index] = surfaces_[index];
    if (surfaces_ != nullptr) host_.release(surfaces_);
    surfaces_ = list;
    room_ = grown;
    return true;
}

void ExportConnection::drop(uint64_t surface)
{
    AV_SCOPE("ExportConnection::drop");
    for (uint32_t index = 0u; index < held_; index++) {
        if (surfaces_[index] != surface) continue;
        surfaces_[index] = surfaces_[--held_];
        return;
    }
}

/* Held surfaces are named, once each; a connection that serves nothing holds
 * none and has no descriptor. */
bool ExportConnection::invariants_hold() const
{
    AV_SCOPE("ExportConnection::invariants_hold");
    if (held_ > room_) return false;
    if (thread_ == nullptr && (descriptor_ != -1 || held_ != 0u)) return false;
    for (uint32_t index = 0u; index < held_; index++) {
        if (surfaces_[index] == 0u) return false;
        for (uint32_t other = index + 1u; other < held_; other++)
            if (surfaces_[other] == surfaces_[index]) return false;
    }
    return true;
}

}  // namespace av
