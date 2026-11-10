#include "icd/opencl/guest_cl_context.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/icd_connection.hpp"
#include "guest/transport/guest_connection.hpp"
#include "core/log/keyed_log.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"
#include "icd/common/arena_region.hpp"

namespace av::clicd {

GuestClContext::GuestClContext(icd::IcdConnection &connection, BlockSource &memory,
                               ThreadLock &send_lock, ThreadLock &blocks_lock, KeyedLog &lines,
                               icd::SharedHandles &handles)
    : memory_(memory), lines_(lines),
      link_(connection), loss_(lines), gl_share_(0u), wakes_(false),
      channel_(memory, lines, icd::ChannelShape(SubApi::opencl, ring_order, reply_bytes, staging_bytes, batch_bytes, arena_bytes),
               send_lock),
      blocks_(memory, blocks_lock), orphaned_work_(false), staging_(memory, blocks_lock),
      region_(),
      svm_(memory), external_(handles, memory, lines), d3d11_(memory), formats_(), mappings_(memory), properties_(), notify_(), state_(State::created)
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
        lines_.write(wakes_ ? "the driver wakes this context's waits"
                            : "this context's waits ask on a timer; the driver does not wake them");
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
    const Status opened = channel_.open(link_.device(), GlContextOptions().sharing(gl_share));
    if (opened != Status::ok) return opened;
    if (!channel_.has_arena()) {
        lines_.write("buffers are staged; a map copies in and out");
        return Status::ok;
    }
    if (region_.open(channel_.wire(), staging_reserve + blocks_reserve)) {
        staging_.lend(region_.offset(), region_.view(), staging_reserve);
        blocks_.lend(region_.offset() + staging_reserve, region_.view() + staging_reserve, blocks_reserve);
    }
    lines_.write("buffers map through the blob arena; no copy");
    return Status::ok;
}

void GuestClContext::teardown()
{
    AV_SCOPE("GuestClContext::teardown");
    gl_share_ = 0u;
    wakes_ = false;
    mappings_.close_all();
    svm_.give_back_all();
    if (state_ == State::made && channel_.opened()) external_.release_all(channel_.wire());
    blocks_.lend(0u, nullptr, 0u);
    staging_.lend(0u, nullptr, 0u);
    region_.close();
    channel_.close(state_ == State::lost);
    link_.release();
}

bool GuestClContext::exhausted() const
{
    AV_SCOPE("GuestClContext::exhausted");
    return channel_.exhausted();
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
    if (state_ != State::made || !channel_.has_arena()) return Status::invalid_state;
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
    if (link_.lost(channel_)) {
        state_ = State::lost;
        loss_.lose(channel_.fault_code());
        static_cast<void>(loss_.refuses("an OpenCL call"));
        notify_.say("avcl: the context's device was lost: every call on it now answers CL_OUT_OF_RESOURCES");
        return nullptr;
    }
    return &channel_.wire();
}

TransportResult GuestClContext::flush()
{
    AV_SCOPE("GuestClContext::flush");
    if (state_ != State::made) return TransportResult::failed;
    return channel_.flush();
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

ClFormatCache &GuestClContext::formats()
{
    AV_SCOPE("GuestClContext::formats");
    return formats_;
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
    return channel_.has_arena();
}

MappedRegion *GuestClContext::take_mapping(uint64_t buffer, uint64_t host, size_t offset, size_t bytes,
                                      cl_map_flags flags)
{
    AV_SCOPE("GuestClContext::take_mapping");
    if (host == 0u) return nullptr;
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
    if (!channel_.invariants_hold()) return false;
    if (loss_.lost() != (state_ == State::lost) || !loss_.invariants_hold()) return false;
    if (state_ == State::created) {
        return !channel_.opened() && !link_.held();
    }
    return channel_.opened() && link_.held();
}

}
