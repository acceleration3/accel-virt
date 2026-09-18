#include "icd/opencl/guest_cl_context.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/common/log_sink.hpp"
#include "proto/context/sub_api.hpp"

namespace av::clicd {

GuestClContext::GuestClContext(icd::IcdConnection &connection, BlockSource &memory,
                               icd::ThreadLock &send_lock, icd::LogSink &log)
    : connection_(connection), memory_(memory), send_lock_(send_lock), log_(log),
      link_(icd::IcdConnection::no_link), staging_(nullptr), transport_(), locked_(), arena_(),
      svm_(memory), state_(State::created)
{
    AV_SCOPE("GuestClContext::GuestClContext");
}

GuestClContext::~GuestClContext()
{
    AV_SCOPE("GuestClContext::~GuestClContext");
    teardown();
}

Status GuestClContext::make()
{
    AV_SCOPE("GuestClContext::make");
    if (state_ != State::created) return Status::invalid_state;
    ScopeExit    undo([&] { teardown(); });
    const Status built = build();
    if (built != Status::ok) return built;
    undo.dismiss();
    state_ = State::made;
    return Status::ok;
}

Status GuestClContext::build()
{
    AV_SCOPE("GuestClContext::build");
    if (connection_.acquire(link_) != Status::ok) return Status::not_supported;
    staging_ = static_cast<uint8_t *>(memory_.allocate(staging_bytes, Pool::paged));
    if (staging_ == nullptr) return Status::no_memory;
    /* Both storages were emptied by teardown or never filled. The record's
     * reserved tail stays zero: it is OpenGL's, and KmtTransport refuses a
     * non-default one on any other sub-API without asking the driver. */
    static_cast<void>(transport_.construct(connection_.device(link_), SubApi::opencl, ring_order,
                                           reply_bytes, staging_, staging_bytes));
    const Status initialized = transport_.object().initialize();
    if (initialized != Status::ok) {
        log_.write("rvkcl: the driver refused the context");
        return initialized;
    }
    static_cast<void>(locked_.construct(transport_.object(), send_lock_));
    static_cast<void>(arena_.construct(connection_.device(link_), transport_.object()));
    if (arena_.object().open(arena_bytes) != Status::ok) {
        /* Two different things fail this, and only one of them is survivable.
         * A device that will not import leaves the context perfectly usable
         * and bulk data crosses inline; a transport that faulted while asking
         * has taken the context with it, and carrying on would hand the
         * application a context whose every call is already lost. */
        if (transport_.object().faulted()) {
            log_.write("rvkcl: the context faulted opening its blob arena");
            return Status::invalid_state;
        }
        log_.write("rvkcl: no blob arena for the context; bulk data crosses inline");
    }
    return Status::ok;
}

void GuestClContext::teardown()
{
    AV_SCOPE("GuestClContext::teardown");
    svm_.give_back_all();
    if (arena_.constructed()) {
        /* A lost context's import is forgotten rather than released: the
         * device frees it with the context, and asking a faulted transport
         * would only fail. */
        /* Neither answer changes what is torn down next; a close that could
         * not be told to the device is a close the device does with the
         * context. */
        if (state_ == State::lost) {
            static_cast<void>(arena_.object().abandon());
        } else {
            static_cast<void>(arena_.object().close());
        }
        arena_.destroy();
    }
    if (locked_.constructed()) locked_.destroy();
    if (transport_.constructed()) {
        transport_.object().teardown();
        transport_.destroy();
    }
    if (staging_ != nullptr) {
        memory_.release(staging_);
        staging_ = nullptr;
    }
    if (link_ != icd::IcdConnection::no_link) {
        connection_.release(link_);
        link_ = icd::IcdConnection::no_link;
    }
}

GuestClContext::State GuestClContext::state() const
{
    AV_SCOPE("GuestClContext::state");
    return state_;
}

ClientTransport *GuestClContext::wire()
{
    AV_SCOPE("GuestClContext::wire");
    if (state_ != State::made) return nullptr;
    /* The one place loss is noticed, because it is the one place every call
     * goes through. The device is lost with it: a context whose transport
     * faulted has taken its whole device down (icd::IcdConnection). */
    if (transport_.object().faulted()) {
        state_ = State::lost;
        connection_.lose(link_);
        log_.write("rvkcl: the context faulted; it is lost");
        return nullptr;
    }
    return &locked_.object();
}

bool GuestClContext::lost() const
{
    AV_SCOPE("GuestClContext::lost");
    return state_ == State::lost;
}

SvmTable &GuestClContext::svm()
{
    AV_SCOPE("GuestClContext::svm");
    return svm_;
}

bool GuestClContext::has_arena() const
{
    AV_SCOPE("GuestClContext::has_arena");
    return arena_.constructed() && arena_.object().lending();
}

void GuestClContext::report(StateWriter &writer) const
{
    AV_SCOPE("GuestClContext::report");
    writer.open("context", 0u);
    writer.number("state", static_cast<uint32_t>(state_));
    writer.number("link", link_);
    writer.flag("arena", has_arena());
    svm_.report(writer);
    writer.close();
}

bool GuestClContext::invariants_hold() const
{
    AV_SCOPE("GuestClContext::invariants_hold");
    if (!svm_.invariants_hold()) return false;
    if (state_ == State::created) {
        /* Nothing is held before a make, and a make that failed held nothing
         * after it either. */
        return !transport_.constructed() && !locked_.constructed() && staging_ == nullptr &&
               link_ == icd::IcdConnection::no_link;
    }
    if (!transport_.constructed() || !locked_.constructed()) return false;
    if (staging_ == nullptr || link_ == icd::IcdConnection::no_link) return false;
    return transport_.object().invariants_hold();
}

}  // namespace av::clicd
