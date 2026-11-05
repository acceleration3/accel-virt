#include "icd/opengl/sliced_buffers.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/gpu_timeline.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/generated/glmem/glmem_dispatch_client.h"

namespace av::glicd {

namespace {

constexpr uint32_t known_access = 0x00FFu;
constexpr uint32_t both = BufferMapping::map_read | BufferMapping::map_write;

}

SlicedBuffers::SlicedBuffers(BlockSource &memory, icd::HostChannel &channel, icd::GpuTimeline &timeline,
                             ThreadLock &lock, DriverLog &lines, const void *owner)
    : memory_(memory), channel_(channel), timeline_(timeline), lock_(lock), lines_(lines), owner_(owner), clock_(),
      streams_{}, rows_(memory, Pool::paged), granted_(false), stopped_(false)
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
    rows_.clear();
    for (ObjectStorage<icd::StreamMemory> &stream : streams_) stream.destroy();
    granted_ = false;
}

uint32_t SlicedBuffers::stream_index(HostMemoryRequest::Kind kind) const
{
    AV_SCOPE("SlicedBuffers::stream_index");
    return static_cast<uint32_t>(kind) - 1u;
}

icd::StreamMemory *SlicedBuffers::stream_of(HostMemoryRequest::Kind kind)
{
    AV_SCOPE("SlicedBuffers::stream_of");
    if (!granted_ || channel_.arena_view() == nullptr) return nullptr;
    ObjectStorage<icd::StreamMemory> &made = streams_[stream_index(kind)];
    if (!made.constructed() &&
        made.construct(memory_, channel_, owner_, timeline_, lock_, clock_, lines_, kind, alignment) != Status::ok)
        return nullptr;
    return &made.object();
}

icd::StreamMemory *SlicedBuffers::made_stream(HostMemoryRequest::Kind kind)
{
    AV_SCOPE("SlicedBuffers::made_stream");
    ObjectStorage<icd::StreamMemory> &made = streams_[stream_index(kind)];
    return made.constructed() ? &made.object() : nullptr;
}

bool SlicedBuffers::offers(uint64_t size, uint32_t flags) const
{
    AV_SCOPE("SlicedBuffers::offers");
    if (!granted_ || stopped_ || size == 0u || size > most_bytes) return false;
    constexpr uint32_t known_storage = both | BufferMapping::map_persistent | gl_map_coherent_bit |
                                       gl_dynamic_storage_bit | gl_client_storage_bit;
    if ((flags & ~known_storage) != 0u || (flags & both) == 0u) return false;
    if ((flags & gl_map_coherent_bit) != 0u && (flags & BufferMapping::map_persistent) == 0u) return false;
    return (flags & BufferMapping::map_persistent) != 0u && (flags & gl_dynamic_storage_bit) == 0u;
}

Status SlicedBuffers::place(ClientTransport &wire, uint32_t buffer, uint64_t size, uint32_t flags, const void *data)
{
    AV_SCOPE("SlicedBuffers::place");
    if (!offers(size, flags)) return Status::not_supported;
    if (buffer == 0u || rows_.find(buffer) != nullptr) return Status::conflict;
    const HostMemoryRequest::Kind kind =
        (flags & BufferMapping::map_read) != 0u ? HostMemoryRequest::Kind::readback : HostMemoryRequest::Kind::upload;
    icd::StreamMemory *const stream = stream_of(kind);
    if (stream == nullptr) return Status::not_supported;
    if (rows_.reserve(rows_.count() + 1u) != Status::ok) return Status::no_memory;
    icd::StreamMemory::Slice slice = {};
    const Status             taken = stream->take_slice(size, slice);
    if (taken != Status::ok) return taken;
    if (data != nullptr) __builtin_memcpy(stream->view(slice), data, static_cast<size_t>(size));
    uint32_t row = 0u;
    static_cast<void>(rows_.insert(buffer, Row{slice, size, flags, 0u, 0u, 0u, kind}, row));
    if (glmemproto::av_glmem_PlaceBufferSlice(wire, buffer, stream->memory(slice), stream->offset_in_block(slice), size))
        return Status::ok;
    rows_.erase(buffer);
    stream->give_back(slice);
    return Status::invalid_state;
}

void SlicedBuffers::forget(ClientTransport &wire, int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("SlicedBuffers::forget");
    for (int32_t at = 0; buffers != nullptr && at < count; at++) {
        Row row = {};
        if (!rows_.take(buffers[at], row)) continue;
        static_cast<void>(glmemproto::av_glmem_ForgetBufferSlice(wire, buffers[at]));
        icd::StreamMemory *const stream = made_stream(row.kind_);
        if (stream != nullptr) stream->retire(row.slice_);
    }
}

void SlicedBuffers::trim()
{
    AV_SCOPE("SlicedBuffers::trim");
    for (ObjectStorage<icd::StreamMemory> &stream : streams_)
        if (stream.constructed()) stream.object().trim();
}

bool SlicedBuffers::holds(uint32_t buffer) const
{
    AV_SCOPE("SlicedBuffers::holds");
    return buffer != 0u && rows_.find(buffer) != nullptr;
}

uint32_t SlicedBuffers::access_error(const Row &row, int64_t offset, int64_t length, uint32_t access) const
{
    AV_SCOPE("SlicedBuffers::access_error");
    constexpr uint32_t not_with_read =
        BufferMapping::map_invalidate_range | BufferMapping::map_invalidate_buffer | BufferMapping::map_unsynchronized;
    if ((access & ~known_access) != 0u || offset < 0 || length == 0 || length < whole_buffer) return gl_invalid_value;
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

uint32_t SlicedBuffers::map(uint32_t buffer, int64_t offset, int64_t length, uint32_t access, void *&pointer)
{
    AV_SCOPE("SlicedBuffers::map");
    pointer = nullptr;
    Row *const row = rows_.find(buffer);
    icd::StreamMemory *const stream = row != nullptr ? made_stream(row->kind_) : nullptr;
    if (row == nullptr || stream == nullptr || row->access_ != 0u) return gl_invalid_operation;
    const uint32_t refused = access_error(*row, offset, length, access);
    if (refused != gl_no_error) return refused;
    constexpr uint32_t unsynchronized = BufferMapping::map_persistent | BufferMapping::map_unsynchronized;
    if ((access & unsynchronized) == 0u && !timeline_.wait(timeline_.recording())) return gl_invalid_operation;
    const uint64_t at = static_cast<uint64_t>(offset);
    row->access_ = access;
    row->map_offset_ = at;
    row->map_bytes_ = length == whole_buffer ? row->size_ - at : static_cast<uint64_t>(length);
    pointer = stream->view(row->slice_) + at;
    return gl_no_error;
}

uint32_t SlicedBuffers::unmap(uint32_t buffer)
{
    AV_SCOPE("SlicedBuffers::unmap");
    Row *const row = rows_.find(buffer);
    if (row == nullptr || row->access_ == 0u) return gl_invalid_operation;
    row->access_ = 0u;
    row->map_offset_ = 0u;
    row->map_bytes_ = 0u;
    return gl_no_error;
}

uint32_t SlicedBuffers::mark(uint32_t buffer, int64_t offset, int64_t length)
{
    AV_SCOPE("SlicedBuffers::mark");
    const Row *const row = rows_.find(buffer);
    if (offset < 0 || length < 0) return gl_invalid_value;
    if (row == nullptr || row->access_ == 0u || (row->access_ & BufferMapping::map_flush_explicit) == 0u)
        return gl_invalid_operation;
    const uint64_t at = static_cast<uint64_t>(offset);
    const uint64_t bytes = static_cast<uint64_t>(length);
    return at > row->map_bytes_ || bytes > row->map_bytes_ - at ? gl_invalid_value : gl_no_error;
}

uint32_t SlicedBuffers::read(uint32_t buffer, int64_t offset, int64_t size, void *data)
{
    AV_SCOPE("SlicedBuffers::read");
    Row *const row = rows_.find(buffer);
    icd::StreamMemory *const stream = row != nullptr ? made_stream(row->kind_) : nullptr;
    if (row == nullptr || stream == nullptr) return gl_invalid_operation;
    if (row->access_ != 0u && (row->access_ & BufferMapping::map_persistent) == 0u) return gl_invalid_operation;
    if (offset < 0 || size < 0) return gl_invalid_value;
    const uint64_t at = static_cast<uint64_t>(offset);
    const uint64_t bytes = static_cast<uint64_t>(size);
    if (at > row->size_ || bytes > row->size_ - at) return gl_invalid_value;
    if (bytes == 0u || data == nullptr) return gl_no_error;
    if (!timeline_.wait(timeline_.recording())) return gl_invalid_operation;
    __builtin_memcpy(data, stream->view(row->slice_) + at, static_cast<size_t>(bytes));
    return gl_no_error;
}

void *SlicedBuffers::pointer_of(uint32_t buffer) const
{
    AV_SCOPE("SlicedBuffers::pointer_of");
    const Row *const row = rows_.find(buffer);
    if (row == nullptr || row->access_ == 0u) return nullptr;
    const ObjectStorage<icd::StreamMemory> &made = streams_[stream_index(row->kind_)];
    return made.constructed() ? made.object().view(row->slice_) + row->map_offset_ : nullptr;
}

bool SlicedBuffers::mapping(uint32_t buffer, uint32_t &access, uint64_t &offset, uint64_t &bytes) const
{
    AV_SCOPE("SlicedBuffers::mapping");
    const Row *const row = rows_.find(buffer);
    if (row == nullptr) return false;
    access = row->access_;
    offset = row->map_offset_;
    bytes = row->map_bytes_;
    return true;
}

bool SlicedBuffers::storage_flags(uint32_t buffer, int64_t &value) const
{
    AV_SCOPE("SlicedBuffers::storage_flags");
    const Row *const row = rows_.find(buffer);
    if (row == nullptr) return false;
    value = static_cast<int64_t>(row->flags_);
    return true;
}

uint32_t SlicedBuffers::held() const
{
    AV_SCOPE("SlicedBuffers::held");
    return rows_.count();
}

void SlicedBuffers::report(StateWriter &writer) const
{
    AV_SCOPE("SlicedBuffers::report");
    writer.open("sliced_buffers", 0u);
    writer.flag("granted", granted_);
    writer.flag("stopped", stopped_);
    writer.number("held", held());
    for (const ObjectStorage<icd::StreamMemory> &stream : streams_)
        if (stream.constructed()) writer.number("open_bytes", stream.object().open_bytes());
    writer.close();
}

bool SlicedBuffers::invariants_hold() const
{
    AV_SCOPE("SlicedBuffers::invariants_hold");
    bool sound = true;
    rows_.for_each([&sound](uint32_t buffer, const Row &row) {
        sound = sound && buffer != 0u && row.size_ != 0u && row.size_ <= row.slice_.bytes_ &&
                row.map_offset_ + row.map_bytes_ <= row.size_ && (row.access_ != 0u || row.map_bytes_ == 0u);
    });
    for (const ObjectStorage<icd::StreamMemory> &stream : streams_)
        sound = sound && (!stream.constructed() || stream.object().invariants_hold());
    return sound && rows_.invariants_hold() && (granted_ || rows_.count() == 0u);
}

}
