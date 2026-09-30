#include "icd/opencl/guest_cl_context.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/icd_connection.hpp"
#include "guest/transport/guest_connection.hpp"
#include "icd/common/log_sink.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"

namespace av::clicd {

GuestClContext::GuestClContext(icd::IcdConnection &connection, BlockSource &memory,
                               icd::ThreadLock &send_lock, icd::LogSink &log,
                               icd::SharedHandles &handles)
    : connection_(connection), memory_(memory), send_lock_(send_lock), log_(log),
      link_(icd::IcdConnection::no_link), gl_share_(0u), staging_(nullptr), batch_block_(nullptr), transport_(), batched_(), locked_(), arena_(),
      svm_(memory), external_(handles, log), mappings_(), properties_(), state_(State::created)
{
    AV_SCOPE("GuestClContext::GuestClContext");
}

GuestClContext::~GuestClContext()
{
    AV_SCOPE("GuestClContext::~GuestClContext");
    teardown();
}

Status GuestClContext::make(uint32_t gl_share)
{
    AV_SCOPE("GuestClContext::make");
    if (state_ != State::created) return Status::invalid_state;
    ScopeExit    undo([&] { teardown(); });
    const Status built = build(gl_share);
    if (built != Status::ok) return built;
    undo.dismiss();
    gl_share_ = gl_share;
    state_ = State::made;
    return Status::ok;
}

bool GuestClContext::shares_gl() const
{
    AV_SCOPE("GuestClContext::shares_gl");
    return state_ == State::made && gl_share_ != 0u;
}

void GuestClContext::keep_properties(const cl_context_properties *properties)
{
    AV_SCOPE("GuestClContext::keep_properties");
    static_cast<void>(properties_.read(properties));
}

uint32_t GuestClContext::properties(cl_context_properties *out, uint32_t capacity) const
{
    AV_SCOPE("GuestClContext::properties");
    return properties_.lay_out(out, capacity);
}

Status GuestClContext::build(uint32_t gl_share)
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
    const Status initialized = transport_.object().initialize(GlContextOptions().sharing(gl_share));
    if (initialized != Status::ok) {
        log_.write("rvkcl: the driver refused the context");
        return initialized;
    }
    batch_block_ = static_cast<uint8_t *>(memory_.allocate(batch_bytes, Pool::paged));
    if (batch_block_ == nullptr) return Status::no_memory;
    static_cast<void>(batched_.construct(transport_.object(), batch_block_, batch_bytes, staging_bytes));
    static_cast<void>(locked_.construct(batched_.object(), send_lock_));
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
        log_.write("rvkcl: buffers are staged; a map copies in and out");
        return Status::ok;
    }
    /* The arena is the memory a mappable buffer is made over, on both sides:
     * the device holds it as one import, so an offset names the same bytes
     * here and there and a map costs no copy. */
    blocks_.lend(arena_.object().view(), arena_.object().bytes());
    log_.write("rvkcl: buffers map through the blob arena; no copy");
    return Status::ok;
}

void GuestClContext::teardown()
{
    AV_SCOPE("GuestClContext::teardown");
    gl_share_ = 0u;
    /* A buffer released with a mapping live keeps the mapping's memory until
     * the unmap, so the blocks go here rather than with the buffer. */
    for (uint32_t slot = 0u; slot < max_mappings; ++slot) give_back_mapping(mappings_[slot]);
    svm_.give_back_all();
    /* Before the transport goes, and only while it can still carry a release:
     * the device counts this context as a holder of every object it opened,
     * and a lost context's are released by the device with the context. */
    if (state_ == State::made && locked_.constructed()) external_.release_all(locked_.object());
    /* What is still held reaches the host before its context goes. */
    if (state_ == State::made && batched_.constructed()) static_cast<void>(batched_.object().flush());
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
    /* After the arena, never before: a block naming memory that is gone is
     * an address this context must not be able to hand out. */
    blocks_.lend(nullptr, 0u);
    if (locked_.constructed()) locked_.destroy();
    if (batched_.constructed()) batched_.destroy();
    if (batch_block_ != nullptr) {
        memory_.release(batch_block_);
        batch_block_ = nullptr;
    }
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
     * goes through. A faulted transport takes its whole device down, so every
     * other context on the link finds out here too, at its own next call -
     * which is what makes a sibling stop sending rather than send into a
     * device that is gone (icd::IcdConnection). */
    if (transport_.object().faulted()) connection_.lose(link_);
    if (connection_.lost(link_)) {
        state_ = State::lost;
        log_.write("rvkcl: the context's device faulted; the context is lost");
        return nullptr;
    }
    return &locked_.object();
}

ClArenaBlocks &GuestClContext::blocks()
{
    AV_SCOPE("GuestClContext::blocks");
    return blocks_;
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

ClExternalMemory &GuestClContext::external()
{
    AV_SCOPE("GuestClContext::external");
    return external_;
}

uint32_t GuestClContext::guest_device() const
{
    AV_SCOPE("GuestClContext::guest_device");
    if (link_ == icd::IcdConnection::no_link) return 0u;
    return connection_.device(link_).device();
}

bool GuestClContext::has_arena() const
{
    AV_SCOPE("GuestClContext::has_arena");
    return arena_.constructed() && arena_.object().lending();
}

uint8_t *GuestClContext::take_mapping(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                                      cl_map_flags flags)
{
    AV_SCOPE("GuestClContext::take_mapping");
    if (buffer == 0u || host == 0u || bytes == 0u) return nullptr;
    uint32_t slot = 0u;
    while (slot < max_mappings && mappings_[slot].held()) ++slot;
    if (slot == max_mappings) return nullptr;
    uint8_t *const block = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    if (block == nullptr) return nullptr;
    mappings_[slot].take(buffer, host, offset, bytes, flags, block, true);
    return block;
}

MappedRegion *GuestClContext::take_shared_mapping(uint64_t buffer, uint64_t host, size_t offset,
                                                  size_t bytes, uint8_t *at)
{
    AV_SCOPE("GuestClContext::take_shared_mapping");
    if (buffer == 0u || host == 0u || bytes == 0u || at == nullptr) return nullptr;
    uint32_t slot = 0u;
    while (slot < max_mappings && mappings_[slot].held()) ++slot;
    if (slot == max_mappings) return nullptr;
    /* Not staged: no block was allocated for it, so none is freed with it,
     * and the unmap has nothing to write back. The flags are the map's, but
     * they change nothing here - what a write through this mapping reaches
     * is the buffer itself. */
    mappings_[slot].take(buffer, host, offset, bytes, 0u, at, false);
    return &mappings_[slot];
}

MappedRegion *GuestClContext::mapping_at(const void *pointer)
{
    AV_SCOPE("GuestClContext::mapping_at");
    for (uint32_t slot = 0u; slot < max_mappings; ++slot) {
        if (mappings_[slot].answers(pointer)) return &mappings_[slot];
    }
    return nullptr;
}

void GuestClContext::give_back_mapping(MappedRegion &region)
{
    AV_SCOPE("GuestClContext::give_back_mapping");
    if (!region.held()) return;
    if (region.staged()) memory_.release(region.at());
    region.give_up();
}

uint32_t GuestClContext::mappings() const
{
    AV_SCOPE("GuestClContext::mappings");
    uint32_t held = 0u;
    for (uint32_t slot = 0u; slot < max_mappings; ++slot) {
        if (mappings_[slot].held()) ++held;
    }
    return held;
}

void GuestClContext::report(StateWriter &writer) const
{
    AV_SCOPE("GuestClContext::report");
    writer.open("context", 0u);
    writer.number("state", static_cast<uint32_t>(state_));
    writer.number("link", link_);
    writer.flag("arena", has_arena());
    writer.number("mappings", mappings());
    svm_.report(writer);
    external_.report(writer);
    writer.close();
}

bool GuestClContext::invariants_hold() const
{
    AV_SCOPE("GuestClContext::invariants_hold");
    if (!svm_.invariants_hold() || !external_.invariants_hold()) return false;
    for (uint32_t slot = 0u; slot < max_mappings; ++slot) {
        if (!mappings_[slot].invariants_hold()) return false;
        /* No two live mappings answer the same pointer: an unmap would then
         * have two mappings to choose between. */
        for (uint32_t other = slot + 1u; other < max_mappings; ++other) {
            if (mappings_[slot].held() && mappings_[other].held() &&
                mappings_[slot].at() == mappings_[other].at()) {
                return false;
            }
        }
    }
    /* The transport and the lock around it are constructed together and torn
     * down together, so one without the other is a make or a teardown that
     * stopped halfway - and checking them apart below would be checking a
     * state neither ever produces. */
    if (transport_.constructed() != locked_.constructed()) return false;
    if (state_ == State::created) {
        /* Nothing is held before a make, and a make that failed held nothing
         * after it either. */
        return !transport_.constructed() && staging_ == nullptr &&
               link_ == icd::IcdConnection::no_link;
    }
    if (!transport_.constructed()) return false;
    if (staging_ == nullptr || link_ == icd::IcdConnection::no_link) return false;
    return transport_.object().invariants_hold();
}

}  // namespace av::clicd
