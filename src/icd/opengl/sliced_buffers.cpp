#include "icd/opengl/sliced_buffers.hpp"

#include "core/platform/block_source.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/gpu_timeline.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/opengl/buffer_clear_pattern.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/generated/glmem/glmem_dispatch_client.h"

namespace av::glicd {

namespace {

constexpr uint32_t known_access = 0x00FFu;
constexpr uint32_t both = BufferMapping::map_read | BufferMapping::map_write;

}

SlicedBuffers::SlicedBuffers(BlockSource &memory, icd::HostChannel &channel, icd::GpuTimeline &timeline,
                             SliceGroups &groups, uint32_t group, DriverLog &lines, const void *owner)
    : memory_(memory), channel_(channel), timeline_(timeline), groups_(groups), group_(group), lines_(lines),
      owner_(owner), pins_(0u), clock_(), streams_{}, granted_(false), stopped_(false)
{
    AV_SCOPE("SlicedBuffers::SlicedBuffers");
}

void SlicedBuffers::grant(Kmt &kmt)
{
    AV_SCOPE("SlicedBuffers::grant");
    clock_.bind(kmt);
    granted_ = true;
}

void SlicedBuffers::stop_placing()
{
    AV_SCOPE("SlicedBuffers::stop_placing");
    stopped_ = true;
}

void SlicedBuffers::release()
{
    AV_SCOPE("SlicedBuffers::release");
    {
        const ThreadLockHold hold(groups_.lock());
        groups_.drop_owner(this);
        groups_.withdraw(this);
    }
    while (__atomic_load_n(&pins_, __ATOMIC_ACQUIRE) != 0u) {
    }
    for (ObjectStorage<icd::StreamMemory> &stream : streams_) stream.destroy();
    granted_ = false;
}

icd::StreamMemory *SlicedBuffers::stream_of(uint32_t stream)
{
    AV_SCOPE("SlicedBuffers::stream_of");
    if (!granted_ || channel_.arena_view() == nullptr) return nullptr;
    ObjectStorage<icd::StreamMemory> &made = streams_[stream];
    if (!made.constructed() &&
        made.construct(memory_, channel_, owner_, timeline_, groups_.lock(), clock_, lines_, stream_requests_[stream],
                       alignment) != Status::ok)
        return nullptr;
    return &made.object();
}

icd::StreamMemory *SlicedBuffers::made_stream(uint32_t stream)
{
    AV_SCOPE("SlicedBuffers::made_stream");
    ObjectStorage<icd::StreamMemory> &made = streams_[stream];
    return made.constructed() ? &made.object() : nullptr;
}

uint8_t *SlicedBuffers::view_of(const Row &row) const
{
    AV_SCOPE("SlicedBuffers::view_of");
    if (row.owner_ == nullptr || row.stream_ >= stream_kinds) return nullptr;
    const ObjectStorage<icd::StreamMemory> &made = row.owner_->streams_[row.stream_];
    return made.constructed() ? made.object().view(row.slice_) : nullptr;
}

bool SlicedBuffers::offers(int64_t size, uint32_t flags) const
{
    AV_SCOPE("SlicedBuffers::offers");
    if (!granted_ || stopped_ || size <= 0 || static_cast<uint64_t>(size) > most_bytes) return false;
    constexpr uint32_t known_storage = both | BufferMapping::map_persistent | gl_map_coherent_bit |
                                       gl_dynamic_storage_bit | gl_client_storage_bit;
    if ((flags & ~known_storage) != 0u || (flags & both) == 0u) return false;
    if ((flags & gl_map_coherent_bit) != 0u && (flags & BufferMapping::map_persistent) == 0u) return false;
    return (flags & BufferMapping::map_persistent) != 0u && (flags & gl_dynamic_storage_bit) == 0u;
}

Status SlicedBuffers::place(ClientTransport &wire, uint32_t buffer, uint64_t size, uint32_t flags, const void *data)
{
    AV_SCOPE("SlicedBuffers::place");
    if (size > most_bytes || !offers(static_cast<int64_t>(size), flags)) return Status::not_supported;
    if (buffer == 0u || holds(buffer)) return Status::conflict;
    const uint32_t kind = (flags & BufferMapping::map_read) != 0u ? readback_stream : upload_stream;
    icd::StreamMemory *const stream = stream_of(kind);
    if (stream == nullptr) return Status::not_supported;
    icd::StreamMemory::Slice slice = {};
    const Status             taken = stream->take_slice(size, slice);
    if (taken != Status::ok) return taken;
    uint8_t *const own = stream->view(slice);
    if (own == nullptr) {
        stream->give_back(slice);
        return Status::invalid_state;
    }
    if (data != nullptr) __builtin_memcpy(own, data, static_cast<size_t>(size));
    Status inserted = Status::ok;
    {
        const ThreadLockHold hold(groups_.lock());
        inserted = groups_.insert(group_, buffer, Row{this, slice, size, flags, 0u, 0u, 0u, kind});
    }
    if (inserted != Status::ok) {
        stream->give_back(slice);
        return inserted;
    }
    uint32_t   placed = 0u;
    const bool answered = glmemproto::av_glmem_PlaceBufferSlice(wire, buffer, stream->memory(slice),
                                                                 stream->offset_in_block(slice), size, &placed);
    if (answered && placed == 1u) return Status::ok;
    Row undone = {};
    {
        const ThreadLockHold hold(groups_.lock());
        static_cast<void>(groups_.take(group_, buffer, undone));
    }
    stream->give_back(slice);
    return answered ? Status::not_supported : Status::invalid_state;
}

void SlicedBuffers::forget(int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("SlicedBuffers::forget");
    for (int32_t at = 0; buffers != nullptr && at < count; at++) {
        Row row = {};
        {
            const ThreadLockHold hold(groups_.lock());
            if (!groups_.take(group_, buffers[at], row)) continue;
            row.owner_->pin();
        }
        if (!settle_group(row.owner_))
            lines_.write("the GPU work that may use a deleted shared buffer slice could not be waited for");
        row.owner_->release_row(buffers[at], row);
        row.owner_->unpin();
    }
}

void SlicedBuffers::release_row(uint32_t buffer, const Row &row)
{
    AV_SCOPE("SlicedBuffers::release_row");
    if (channel_.opened() && !channel_.lost())
        static_cast<void>(glmemproto::av_glmem_ForgetBufferSlice(channel_.wire(), buffer));
    icd::StreamMemory *const stream = made_stream(row.stream_);
    if (stream != nullptr) stream->retire(row.slice_);
}

void SlicedBuffers::trim()
{
    AV_SCOPE("SlicedBuffers::trim");
    for (ObjectStorage<icd::StreamMemory> &stream : streams_)
        if (stream.constructed()) stream.object().trim();
}

bool SlicedBuffers::snapshot(uint32_t buffer, Row &copy) const
{
    AV_SCOPE("SlicedBuffers::snapshot");
    const ThreadLockHold hold(groups_.lock());
    const Row *const row = groups_.find(group_, buffer);
    if (row == nullptr) return false;
    copy = *row;
    return true;
}

bool SlicedBuffers::holds(uint32_t buffer) const
{
    AV_SCOPE("SlicedBuffers::holds");
    const ThreadLockHold hold(groups_.lock());
    return groups_.find(group_, buffer) != nullptr;
}

uint32_t SlicedBuffers::access_error(const Row &row, int64_t offset, int64_t length, uint32_t access) const
{
    AV_SCOPE("SlicedBuffers::access_error");
    constexpr uint32_t not_with_read =
        BufferMapping::map_invalidate_range | BufferMapping::map_invalidate_buffer | BufferMapping::map_unsynchronized;
    if ((access & ~known_access) != 0u || offset < 0 || length == 0 || (length < 0 && length != whole_buffer)) return gl_invalid_value;
    if ((access & both) == 0u) return gl_invalid_operation;
    if ((access & BufferMapping::map_read) != 0u && (access & not_with_read) != 0u) return gl_invalid_operation;
    if ((access & BufferMapping::map_flush_explicit) != 0u && (access & BufferMapping::map_write) == 0u)
        return gl_invalid_operation;
    if ((access & gl_map_coherent_bit) != 0u && (access & BufferMapping::map_persistent) == 0u)
        return gl_invalid_operation;
    const uint32_t beyond_storage = access & (both | BufferMapping::map_persistent | gl_map_coherent_bit) & ~row.flags_;
    if (beyond_storage != 0u) return gl_invalid_operation;
    const uint64_t at = static_cast<uint64_t>(offset);
    const uint64_t mapped = length == whole_buffer ? row.size_ - (at < row.size_ ? at : row.size_) : static_cast<uint64_t>(length);
    if (at > row.size_ || mapped > row.size_ - at || mapped == 0u) return gl_invalid_value;
    return gl_no_error;
}

uint32_t SlicedBuffers::window_error(const Row &row, uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("SlicedBuffers::window_error");
    if (row.access_ != 0u && (row.access_ & BufferMapping::map_persistent) == 0u) return gl_invalid_operation;
    return offset > row.size_ || bytes > row.size_ - offset ? gl_invalid_operation : gl_no_error;
}

bool SlicedBuffers::wait_recorded()
{
    AV_SCOPE("SlicedBuffers::wait_recorded");
    return timeline_.wait(timeline_.recording());
}

bool SlicedBuffers::settle_group(const SlicedBuffers *except)
{
    AV_SCOPE("SlicedBuffers::settle_group");
    bool     settled = true;
    uint32_t cursor = 0u;
    for (;;) {
        SlicedBuffers *member = nullptr;
        {
            const ThreadLockHold hold(groups_.lock());
            member = groups_.next_member(group_, cursor);
            if (member != nullptr && member != except) member->pin();
        }
        if (member == nullptr) return settled;
        if (member == except) continue;
        settled = member->wait_recorded() && settled;
        member->unpin();
    }
}

void SlicedBuffers::pin()
{
    AV_SCOPE("SlicedBuffers::pin");
    __atomic_add_fetch(&pins_, 1u, __ATOMIC_ACQ_REL);
}

void SlicedBuffers::unpin()
{
    AV_SCOPE("SlicedBuffers::unpin");
    __atomic_sub_fetch(&pins_, 1u, __ATOMIC_ACQ_REL);
}

uint32_t SlicedBuffers::pinned() const
{
    AV_SCOPE("SlicedBuffers::pinned");
    return __atomic_load_n(&pins_, __ATOMIC_ACQUIRE);
}

Status SlicedBuffers::enroll()
{
    AV_SCOPE("SlicedBuffers::enroll");
    const ThreadLockHold hold(groups_.lock());
    return groups_.enroll(this, group_);
}

uint32_t SlicedBuffers::map(uint32_t buffer, int64_t offset, int64_t length, uint32_t access, void *&pointer)
{
    AV_SCOPE("SlicedBuffers::map");
    pointer = nullptr;
    Row row = {};
    if (!snapshot(buffer, row) || row.access_ != 0u) return gl_invalid_operation;
    uint8_t *const view = view_of(row);
    if (view == nullptr) return gl_invalid_operation;
    const uint32_t refused = access_error(row, offset, length, access);
    if (refused != gl_no_error) return refused;
    constexpr uint32_t unsynchronized = BufferMapping::map_persistent | BufferMapping::map_unsynchronized;
    if ((access & unsynchronized) == 0u && !settle_group(nullptr)) return gl_invalid_operation;
    const uint64_t at = static_cast<uint64_t>(offset);
    {
        const ThreadLockHold hold(groups_.lock());
        Row *const live = groups_.find(group_, buffer);
        if (live == nullptr || live->access_ != 0u) return gl_invalid_operation;
        live->access_ = access;
        live->map_offset_ = at;
        live->map_bytes_ = length == whole_buffer ? row.size_ - at : static_cast<uint64_t>(length);
    }
    pointer = view + at;
    return gl_no_error;
}

uint32_t SlicedBuffers::unmap(uint32_t buffer)
{
    AV_SCOPE("SlicedBuffers::unmap");
    const ThreadLockHold hold(groups_.lock());
    Row *const row = groups_.find(group_, buffer);
    if (row == nullptr || row->access_ == 0u) return gl_invalid_operation;
    row->access_ = 0u;
    row->map_offset_ = 0u;
    row->map_bytes_ = 0u;
    return gl_no_error;
}

uint32_t SlicedBuffers::mark(uint32_t buffer, int64_t offset, int64_t length)
{
    AV_SCOPE("SlicedBuffers::mark");
    Row row = {};
    const bool found = snapshot(buffer, row);
    if (offset < 0 || length < 0) return gl_invalid_value;
    if (!found || row.access_ == 0u || (row.access_ & BufferMapping::map_flush_explicit) == 0u)
        return gl_invalid_operation;
    const uint64_t at = static_cast<uint64_t>(offset);
    const uint64_t bytes = static_cast<uint64_t>(length);
    return at > row.map_bytes_ || bytes > row.map_bytes_ - at ? gl_invalid_value : gl_no_error;
}

uint32_t SlicedBuffers::read(uint32_t buffer, int64_t offset, int64_t size, void *data)
{
    AV_SCOPE("SlicedBuffers::read");
    Row row = {};
    if (!snapshot(buffer, row)) return gl_invalid_operation;
    const uint8_t *const view = view_of(row);
    if (view == nullptr) return gl_invalid_operation;
    if (row.access_ != 0u && (row.access_ & BufferMapping::map_persistent) == 0u) return gl_invalid_operation;
    if (offset < 0 || size < 0) return gl_invalid_value;
    const uint64_t at = static_cast<uint64_t>(offset);
    const uint64_t bytes = static_cast<uint64_t>(size);
    if (at > row.size_ || bytes > row.size_ - at) return gl_invalid_value;
    if (bytes == 0u || data == nullptr) return gl_no_error;
    if (!settle_group(nullptr)) return gl_invalid_operation;
    __builtin_memcpy(data, view + at, static_cast<size_t>(bytes));
    return gl_no_error;
}

uint32_t SlicedBuffers::unpack_source(uint32_t buffer, uint64_t offset, uint64_t bytes, const uint8_t *&data)
{
    AV_SCOPE("SlicedBuffers::unpack_source");
    data = nullptr;
    Row row = {};
    if (!snapshot(buffer, row)) return gl_invalid_operation;
    const uint32_t refused = window_error(row, offset, bytes);
    if (refused != gl_no_error) return refused;
    const uint8_t *const view = view_of(row);
    if (view == nullptr || !settle_group(nullptr)) return gl_invalid_operation;
    data = view + offset;
    return gl_no_error;
}

uint32_t SlicedBuffers::pack_target(uint32_t buffer, uint64_t offset, uint64_t bytes, uint8_t *&data)
{
    AV_SCOPE("SlicedBuffers::pack_target");
    data = nullptr;
    Row row = {};
    if (!snapshot(buffer, row)) return gl_invalid_operation;
    const uint32_t refused = window_error(row, offset, bytes);
    if (refused != gl_no_error) return refused;
    uint8_t *const view = view_of(row);
    if (view == nullptr || !settle_group(nullptr)) return gl_invalid_operation;
    data = view + offset;
    return gl_no_error;
}

uint32_t SlicedBuffers::clear_all(uint32_t buffer, uint32_t internal_format, uint32_t format, uint32_t type,
                                  const void *data)
{
    AV_SCOPE("SlicedBuffers::clear_all");
    Row row = {};
    if (!snapshot(buffer, row)) return gl_invalid_operation;
    return clear_range(buffer, internal_format, 0, static_cast<int64_t>(row.size_), format, type, data);
}

uint32_t SlicedBuffers::clear_range(uint32_t buffer, uint32_t internal_format, int64_t offset, int64_t size,
                                    uint32_t format, uint32_t type, const void *data)
{
    AV_SCOPE("SlicedBuffers::clear_range");
    Row row = {};
    if (!snapshot(buffer, row)) return gl_invalid_operation;
    BufferClearPattern pattern;
    const uint32_t     converted = pattern.convert(internal_format, format, type, data);
    if (converted != gl_no_error) return converted;
    if (offset < 0 || size < 0) return gl_invalid_value;
    const uint64_t at = static_cast<uint64_t>(offset);
    const uint64_t bytes = static_cast<uint64_t>(size);
    if (at > row.size_ || bytes > row.size_ - at || at % pattern.size() != 0u || bytes % pattern.size() != 0u)
        return gl_invalid_value;
    if (row.access_ != 0u && (row.access_ & BufferMapping::map_persistent) == 0u) return gl_invalid_operation;
    uint8_t *const view = view_of(row);
    if (view == nullptr || !settle_group(nullptr)) return gl_invalid_operation;
    pattern.fill(view + at, bytes);
    return gl_no_error;
}

uint32_t SlicedBuffers::invalidate(uint32_t buffer, int64_t offset, int64_t length)
{
    AV_SCOPE("SlicedBuffers::invalidate");
    Row row = {};
    if (!snapshot(buffer, row)) return gl_invalid_operation;
    if (offset < 0 || length < 0) return gl_invalid_value;
    const uint64_t at = static_cast<uint64_t>(offset);
    const uint64_t bytes = static_cast<uint64_t>(length);
    if (at > row.size_ || bytes > row.size_ - at) return gl_invalid_value;
    const bool overlaps = row.access_ != 0u && bytes != 0u && at < row.map_offset_ + row.map_bytes_ &&
                          row.map_offset_ < at + bytes;
    return overlaps && (row.access_ & BufferMapping::map_persistent) == 0u ? gl_invalid_operation : gl_no_error;
}

void *SlicedBuffers::pointer_of(uint32_t buffer) const
{
    AV_SCOPE("SlicedBuffers::pointer_of");
    Row row = {};
    if (!snapshot(buffer, row) || row.access_ == 0u) return nullptr;
    uint8_t *const view = view_of(row);
    return view != nullptr ? view + row.map_offset_ : nullptr;
}

bool SlicedBuffers::mapping(uint32_t buffer, uint32_t &access, uint64_t &offset, uint64_t &bytes) const
{
    AV_SCOPE("SlicedBuffers::mapping");
    Row row = {};
    if (!snapshot(buffer, row)) return false;
    access = row.access_;
    offset = row.map_offset_;
    bytes = row.map_bytes_;
    return true;
}

bool SlicedBuffers::storage_flags(uint32_t buffer, int64_t &value) const
{
    AV_SCOPE("SlicedBuffers::storage_flags");
    Row row = {};
    if (!snapshot(buffer, row)) return false;
    value = static_cast<int64_t>(row.flags_);
    return true;
}

uint32_t SlicedBuffers::held() const
{
    AV_SCOPE("SlicedBuffers::held");
    const ThreadLockHold hold(groups_.lock());
    return groups_.count();
}

uint32_t SlicedBuffers::owned() const
{
    AV_SCOPE("SlicedBuffers::owned");
    const ThreadLockHold hold(groups_.lock());
    return groups_.owned_by(this);
}

uint32_t SlicedBuffers::busy() const
{
    AV_SCOPE("SlicedBuffers::busy");
    const uint32_t rows = owned();
    return rows + pinned();
}

uint32_t SlicedBuffers::group() const
{
    AV_SCOPE("SlicedBuffers::group");
    return group_;
}

void SlicedBuffers::report(StateWriter &writer) const
{
    AV_SCOPE("SlicedBuffers::report");
    writer.open("sliced_buffers", 0u);
    writer.flag("granted", granted_);
    writer.flag("stopped", stopped_);
    writer.number("group", group_);
    writer.number("owned", owned());
    writer.number("held", held());
    for (const ObjectStorage<icd::StreamMemory> &stream : streams_)
        if (stream.constructed()) writer.number("open_bytes", stream.object().open_bytes());
    writer.close();
}

bool SlicedBuffers::invariants_hold() const
{
    AV_SCOPE("SlicedBuffers::invariants_hold");
    bool sound = group_ != 0u;
    for (const ObjectStorage<icd::StreamMemory> &stream : streams_)
        sound = sound && (!stream.constructed() || stream.object().invariants_hold());
    return sound && (granted_ || owned() == 0u);
}

}
