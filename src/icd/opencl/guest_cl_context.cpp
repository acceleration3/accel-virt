#include "icd/opencl/guest_cl_context.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/icd_connection.hpp"
#include "guest/transport/guest_connection.hpp"
#include "core/log/log_sink.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"
#include "icd/common/arena_spans.hpp"

namespace av::clicd {

GuestClContext::GuestClContext(icd::IcdConnection &connection, BlockSource &memory,
                               ThreadLock &send_lock, ThreadLock &blocks_lock, LogSink &log, KeyedLog &lines,
                               icd::SharedHandles &handles)
    : memory_(memory), log_(log),
      link_(connection), loss_(lines), gl_share_(0u), wakes_(false),
      channel_(memory, log, "avcl: ", icd::ChannelShape(SubApi::opencl, ring_order, reply_bytes, staging_bytes, batch_bytes, arena_bytes),
               send_lock),
      blocks_(memory, blocks_lock), orphaned_work_(false), staging_(memory, blocks_lock),
      svm_(memory), external_(handles, memory, log), d3d11_(memory), mappings_(memory), properties_(), notify_(), state_(State::created)
{
    AV_SCOPE("GuestClContext::GuestClContext");
}

GuestClContext::~GuestClContext()
{
    AV_SCOPE("GuestClContext::~GuestClContext");
    teardown();
}

Status GuestClContext::make(uint32_t gl_share, uint64_t wake)
{
    AV_SCOPE("GuestClContext::make");
    if (state_ != State::created) return Status::invalid_state;
    ScopeExit    undo([&] { teardown(); });
    const Status built = build(gl_share);
    if (built != Status::ok) return built;
    undo.dismiss();
    if (wake != 0u) {
        wakes_ = channel_.transport().wake_on_raise(wake) == Status::ok;
        log_.write(wakes_ ? "avcl: the driver wakes this context's waits"
                          : "avcl: this context's waits ask on a timer; the driver does not wake them");
    }
    gl_share_ = gl_share;
    state_ = State::made;
    return Status::ok;
}

bool GuestClContext::wakes() const
{
    AV_SCOPE("GuestClContext::wakes");
    return wakes_;
}

bool GuestClContext::on(const icd::IcdConnection &connection) const
{
    AV_SCOPE("GuestClContext::on");
    return link_.on(connection);
}

bool GuestClContext::shares_gl() const
{
    AV_SCOPE("GuestClContext::shares_gl");
    return state_ == State::made && gl_share_ != 0u;
}

void GuestClContext::notify_with(const ClContextNotify &notify)
{
    AV_SCOPE("GuestClContext::notify_with");
    notify_ = notify;
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
    if (link_.acquire() != Status::ok) return Status::not_supported;
    /* The record's reserved tail stays zero: it is OpenGL's, and
     * KmtTransport refuses a non-default one on any other sub-API without
     * asking the driver. */
    const Status opened = channel_.open(link_.device(), GlContextOptions().sharing(gl_share));
    if (opened != Status::ok) return opened;
    if (channel_.arena_view() == nullptr) {
        log_.write("avcl: buffers are staged; a map copies in and out");
        return Status::ok;
    }
    /* The arena is the memory a mappable buffer is made over, on both sides:
     * the device holds it as one import, so an offset names the same bytes
     * here and there and a map costs no copy. Its top is the transfers' own
     * (ArenaSpans). */
    icd::ArenaSpans spans(channel_);
    uint8_t        *staging = nullptr;
    uint64_t        staged = 0u;
    if (spans.span(staging_reserve, staging, staged)) staging_.lend(staging, staged);
    blocks_.lend(channel_.arena_view(), spans.lent());
    log_.write("avcl: buffers map through the blob arena; no copy");
    return Status::ok;
}

void GuestClContext::teardown()
{
    AV_SCOPE("GuestClContext::teardown");
    gl_share_ = 0u;
    wakes_ = false;
    /* A buffer released with a mapping live keeps the mapping's memory until
     * the unmap, so the blocks go here rather than with the buffer. */
    mappings_.close_all();
    svm_.give_back_all();
    /* Before the transport goes, and only while it can still carry a release:
     * the device counts this context as a holder of every object it opened,
     * and a lost context's are released by the device with the context. */
    if (state_ == State::made && channel_.opened()) external_.release_all(channel_.wire());
    /* Before the arena goes: a block naming memory that is gone is an
     * address this context must not be able to hand out. */
    blocks_.lend(nullptr, 0u);
    staging_.lend(nullptr, 0u);
    /* What is still held reaches the host before its context goes; a lost
     * context's arena import is forgotten rather than released, since the
     * device frees it with the context. */
    channel_.close(state_ == State::lost);
    link_.release();
}

GuestClContext::State GuestClContext::state() const
{
    AV_SCOPE("GuestClContext::state");
    return state_;
}

Status GuestClContext::lend(void *address, uint64_t bytes, HostAllocation *&made)
{
    AV_SCOPE("GuestClContext::lend");
    made = nullptr;
    if (state_ != State::made || channel_.arena_view() == nullptr) return Status::invalid_state;
    return channel_.adopt_memory(address, bytes, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u), this,
                                 made);
}

void GuestClContext::give_back_lent(HostAllocation &lent)
{
    AV_SCOPE("GuestClContext::give_back_lent");
    static_cast<void>(channel_.close_memory(lent));
}

void GuestClContext::retire(const uint64_t *block, HostAllocation *lent)
{
    AV_SCOPE("GuestClContext::retire");
    if (block != nullptr) blocks_.retire(*block);
    if (lent != nullptr) channel_.retire_memory(*lent);
}

void GuestClContext::reclaim()
{
    AV_SCOPE("GuestClContext::reclaim");
    if (__atomic_load_n(&orphaned_work_, __ATOMIC_ACQUIRE)) return;
    blocks_.reclaim();
    channel_.reclaim_retired();
}

void GuestClContext::orphan_work()
{
    AV_SCOPE("GuestClContext::orphan_work");
    __atomic_store_n(&orphaned_work_, true, __ATOMIC_RELEASE);
}

ClientTransport *GuestClContext::wire()
{
    AV_SCOPE("GuestClContext::wire");
    if (state_ != State::made) return nullptr;
    /* The one place loss is noticed, because it is the one place every call
     * goes through (icd::ContextLink). */
    if (link_.lost(channel_)) {
        state_ = State::lost;
        loss_.lose(channel_.fault_code());
        static_cast<void>(loss_.refuses("an OpenCL call"));
        notify_.say("avcl: the context's device was lost: every call on it now answers CL_OUT_OF_RESOURCES");
        return nullptr;
    }
    return &channel_.wire();
}

ClArenaBlocks &GuestClContext::staging()
{
    AV_SCOPE("GuestClContext::staging");
    return staging_;
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

ClD3d11Objects &GuestClContext::d3d11()
{
    AV_SCOPE("GuestClContext::d3d11");
    return d3d11_;
}

uint32_t GuestClContext::guest_device() const
{
    AV_SCOPE("GuestClContext::guest_device");
    if (!link_.held()) return 0u;
    return link_.device().device();
}

bool GuestClContext::has_arena() const
{
    AV_SCOPE("GuestClContext::has_arena");
    return channel_.arena_view() != nullptr;
}

MappedRegion *GuestClContext::take_mapping(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                                      cl_map_flags flags)
{
    AV_SCOPE("GuestClContext::take_mapping");
    if (host == 0u) return nullptr;
    /* A write-invalidate map is never read from the host, and a read may
     * answer short: the copy starts zeroed (OpenMappings::stage). */
    MappedRegion *taken = nullptr;
    if (mappings_.stage(MappedRegion(buffer, host, offset, bytes, flags, MappedRegion::Memory::staged), taken) !=
        Status::ok)
        return nullptr;
    return taken;
}

MappedRegion *GuestClContext::take_image_mapping(uint64_t image, const size_t *origin, const size_t *region,
                                            size_t element_bytes, cl_map_flags flags)
{
    AV_SCOPE("GuestClContext::take_image_mapping");
    const ClImageRegion layout(origin, region, 0u, 0u, element_bytes);
    if (!layout.valid()) return nullptr;
    const size_t  bytes = layout.slice_pitch() * layout.slices();
    MappedRegion *taken = nullptr;
    if (mappings_.stage(MappedRegion(image, origin, region, element_bytes, bytes, flags), taken) != Status::ok)
        return nullptr;
    return taken;
}

MappedRegion *GuestClContext::take_shared_mapping(uint64_t buffer, uint64_t host, size_t offset,
                                                  size_t bytes, uint8_t *at)
{
    AV_SCOPE("GuestClContext::take_shared_mapping");
    if (host == 0u || bytes == 0u) return nullptr;
    /* The flags are the map's, but they change nothing here - what a write
     * through this mapping reaches is the buffer itself. */
    MappedRegion *taken = nullptr;
    if (mappings_.lend(MappedRegion(buffer, host, offset, bytes, 0u, MappedRegion::Memory::shared), at, taken) !=
        Status::ok)
        return nullptr;
    return taken;
}

MappedRegion *GuestClContext::take_application_mapping(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                                                  cl_map_flags flags, uint8_t *at)
{
    AV_SCOPE("GuestClContext::take_application_mapping");
    if (host == 0u || bytes == 0u) return nullptr;
    MappedRegion *taken = nullptr;
    if (mappings_.lend(MappedRegion(buffer, host, offset, bytes, flags, MappedRegion::Memory::application), at,
                       taken) != Status::ok)
        return nullptr;
    return taken;
}

MappedRegion *GuestClContext::mapping_at(uint64_t buffer, const void *pointer)
{
    AV_SCOPE("GuestClContext::mapping_at");
    return mappings_.answering(buffer, pointer);
}

void GuestClContext::give_back_mapping(MappedRegion &region)
{
    AV_SCOPE("GuestClContext::give_back_mapping");
    mappings_.close(region);
}

uint32_t GuestClContext::give_back_mappings_of(uint64_t buffer)
{
    AV_SCOPE("GuestClContext::give_back_mappings_of");
    return mappings_.close_if([buffer](const MappedRegion &region) { return region.object() == buffer; });
}

uint32_t GuestClContext::mappings() const
{
    AV_SCOPE("GuestClContext::mappings");
    return mappings_.count();
}

void GuestClContext::report(StateWriter &writer) const
{
    AV_SCOPE("GuestClContext::report");
    writer.open("context", 0u);
    writer.number("state", static_cast<uint32_t>(state_));
    writer.number("device", guest_device());
    loss_.report(writer);
    writer.flag("arena", has_arena());
    writer.number("mappings", mappings());
    svm_.report(writer);
    external_.report(writer);
    d3d11_.report(writer);
    writer.close();
}

bool GuestClContext::invariants_hold() const
{
    AV_SCOPE("GuestClContext::invariants_hold");
    if (!svm_.invariants_hold() || !external_.invariants_hold() || !d3d11_.invariants_hold()) return false;
    if (!mappings_.invariants_hold()) return false;
    /* The channel and the lock around it are opened together and torn down
     * together, so one without the other is a make or a teardown that
     * stopped halfway. */
    if (!channel_.invariants_hold()) return false;
    if (loss_.lost() != (state_ == State::lost) || !loss_.invariants_hold()) return false;
    if (state_ == State::created) {
        /* Nothing is held before a make, and a make that failed held nothing
         * after it either. */
        return !channel_.opened() && !link_.held();
    }
    return channel_.opened() && link_.held();
}

}  // namespace av::clicd
