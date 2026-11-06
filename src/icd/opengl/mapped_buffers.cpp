#include "icd/opengl/mapped_buffers.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/log/driver_log.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/sliced_buffers.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/submission/channel_counter.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

namespace {

constexpr uint32_t known_access = 0x007Fu;

uint32_t access_error(uint32_t access)
{
    AV_SCOPE("access_error");
    constexpr uint32_t read_write = BufferMapping::map_read | BufferMapping::map_write;
    constexpr uint32_t not_with_read =
        BufferMapping::map_invalidate_range | BufferMapping::map_invalidate_buffer | BufferMapping::map_unsynchronized;
    if ((access & ~known_access) != 0u) return MappedBuffers::gl_invalid_value;
    if ((access & read_write) == 0u) return MappedBuffers::gl_invalid_operation;
    if ((access & BufferMapping::map_read) != 0u && (access & not_with_read) != 0u)
        return MappedBuffers::gl_invalid_operation;
    if ((access & BufferMapping::map_flush_explicit) != 0u && (access & BufferMapping::map_write) == 0u)
        return MappedBuffers::gl_invalid_operation;
    return MappedBuffers::gl_no_error;
}

}

MappedBuffers::MappedBuffers(BlockSource &memory, BufferShadows &shadows, SlicedBuffers &slices, DriverLog &log)
    : shadows_(shadows), slices_(slices), log_(log), transfer_(shadows), mappings_(memory), persistent_writers_(0u), logged_()
{
    AV_SCOPE("MappedBuffers::MappedBuffers");
}

MappedBuffers::~MappedBuffers()
{
    AV_SCOPE("MappedBuffers::~MappedBuffers");
    release();
}

uint32_t MappedBuffers::map(ClientTransport &wire, uint32_t buffer, int64_t offset, int64_t length, uint32_t access,
                            void *&pointer)
{
    AV_SCOPE("MappedBuffers::map");
    pointer = nullptr;
    if (slices_.holds(buffer)) return slices_.map(buffer, offset, length, access, pointer);
    const uint32_t refused = access_error(access);
    if (refused != gl_no_error) return refused;
    if (offset < 0 || length == 0 || (length < 0 && length != whole_buffer)) return gl_invalid_value;
    if (buffer == 0u || find(buffer) != nullptr) return gl_invalid_operation;
    uint64_t known = 0u;
    int64_t  size = -1;
    if (shadows_.size_of(buffer, known)) {
        size = static_cast<int64_t>(known);
    } else {
        glproto::av_glGetNamedBufferParameteri64v(wire, buffer, gl_buffer_size, &size);
        if (size < 0) return gl_invalid_operation;
        shadows_.sized(buffer, static_cast<uint64_t>(size));
    }
    const int64_t mapped = length == whole_buffer ? size : length;
    if (offset > size || mapped > size - offset) return gl_invalid_value;
    if (mapped == 0) return gl_invalid_value;
    if (static_cast<uint64_t>(mapped) > BufferMapping::max_length) return gl_out_of_memory;
    BufferMapping *mapping = nullptr;
    if (mappings_.stage(BufferMapping(buffer, static_cast<uint64_t>(offset), static_cast<uint64_t>(mapped), access),
                        mapping) != Status::ok)
        return gl_out_of_memory;
    transfer_.start(wire, *mapping);
    log_once();
    if (mapping->persistent_writes()) persistent_writers_++;
    if ((access & BufferMapping::map_persistent) != 0u) wire.note(ChannelCounter::copied_mappings, 1u);
    pointer = mapping->at();
    return gl_no_error;
}

uint32_t MappedBuffers::unmap(ClientTransport &wire, uint32_t buffer)
{
    AV_SCOPE("MappedBuffers::unmap");
    if (slices_.holds(buffer)) return slices_.unmap(buffer);
    BufferMapping *const mapping = find(buffer);
    if (mapping == nullptr) return gl_invalid_operation;
    transfer_.write_back(wire, *mapping);
    drop(*mapping);
    return gl_no_error;
}

uint32_t MappedBuffers::mark(uint32_t buffer, int64_t offset, int64_t length)
{
    AV_SCOPE("MappedBuffers::mark");
    if (slices_.holds(buffer)) return slices_.mark(buffer, offset, length);
    BufferMapping *const mapping = find(buffer);
    if (offset < 0 || length < 0) return gl_invalid_value;
    if (mapping == nullptr) return gl_invalid_operation;
    const Status marked = mapping->mark(static_cast<uint64_t>(offset), static_cast<uint64_t>(length));
    if (marked == Status::invalid_state) return gl_invalid_operation;
    return marked == Status::ok ? gl_no_error : gl_invalid_value;
}

void *MappedBuffers::pointer_of(uint32_t buffer) const
{
    AV_SCOPE("MappedBuffers::pointer_of");
    if (slices_.holds(buffer)) return slices_.pointer_of(buffer);
    const BufferMapping *const mapping = find(buffer);
    return mapping != nullptr ? mapping->at() : nullptr;
}

bool MappedBuffers::map_state(uint32_t buffer, uint32_t pname, int64_t &value) const
{
    AV_SCOPE("MappedBuffers::map_state");
    const BufferMapping *mapping = find(buffer);
    uint32_t             access = mapping != nullptr ? mapping->access() : 0u;
    uint64_t             offset = mapping != nullptr ? mapping->offset() : 0u;
    uint64_t             bytes = mapping != nullptr ? mapping->bytes() : 0u;
    static_cast<void>(slices_.mapping(buffer, access, offset, bytes));
    const uint32_t       both = BufferMapping::map_read | BufferMapping::map_write;
    switch (pname) {
    case gl_buffer_storage_flags:
        return buffer == 0u || slices_.storage_flags(buffer, value);
    case gl_buffer_mapped:
        value = mapping != nullptr || access != 0u ? 1 : 0;
        return true;
    case gl_buffer_access_flags:
        value = access;
        return true;
    case gl_buffer_map_offset:
        value = static_cast<int64_t>(offset);
        return true;
    case gl_buffer_map_length:
        value = static_cast<int64_t>(bytes);
        return true;
    case gl_buffer_access:
        value = (access & both) == BufferMapping::map_read    ? gl_read_only
                : (access & both) == BufferMapping::map_write ? gl_write_only
                                                              : gl_read_write;
        return true;
    default:
        return false;
    }
}

void MappedBuffers::flush_persistent(ClientTransport &wire)
{
    AV_SCOPE("MappedBuffers::flush_persistent");
    if (persistent_writers_ == 0u) return;
    mappings_.for_each([this, &wire](BufferMapping &mapping) {
        if (mapping.persistent_writes()) transfer_.write_back(wire, mapping);
    });
}

void MappedBuffers::forget(int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("MappedBuffers::forget");
    if (buffers == nullptr) return;
    for (int32_t at = 0; at < count; at++) {
        BufferMapping *const mapping = find(buffers[at]);
        if (mapping != nullptr) drop(*mapping);
    }
}

void MappedBuffers::release()
{
    AV_SCOPE("MappedBuffers::release");
    mappings_.close_all();
    persistent_writers_ = 0u;
}

uint32_t MappedBuffers::held() const
{
    AV_SCOPE("MappedBuffers::held");
    return mappings_.count();
}

void MappedBuffers::report(StateWriter &writer) const
{
    AV_SCOPE("MappedBuffers::report");
    writer.open("mappings", 0u);
    writer.number("held", held());
    writer.number("persistent_writers", persistent_writers_);
    uint32_t index = 0u;
    mappings_.for_each([&writer, &index](const BufferMapping &mapping) { mapping.report(writer, index++); });
    writer.close();
}

bool MappedBuffers::invariants_hold() const
{
    AV_SCOPE("MappedBuffers::invariants_hold");
    uint32_t writers = 0u;
    mappings_.for_each([&writers](const BufferMapping &mapping) { writers += mapping.persistent_writes() ? 1u : 0u; });
    return writers == persistent_writers_ && mappings_.invariants_hold();
}

BufferMapping *MappedBuffers::find(uint32_t buffer)
{
    AV_SCOPE("MappedBuffers::find");
    return buffer != 0u ? mappings_.find(buffer, 0u) : nullptr;
}

const BufferMapping *MappedBuffers::find(uint32_t buffer) const
{
    AV_SCOPE("MappedBuffers::find");
    return buffer != 0u ? mappings_.find(buffer, 0u) : nullptr;
}

void MappedBuffers::drop(BufferMapping &mapping)
{
    AV_SCOPE("MappedBuffers::drop");
    if (mapping.persistent_writes()) persistent_writers_--;
    mappings_.close(mapping);
}

void MappedBuffers::log_once()
{
    AV_SCOPE("MappedBuffers::log_once");
    if (!logged_.first()) return;
    log_.write("buffer maps are guest copies: filled at map from what the host holds, the bytes that "
               "changed written back at unmap, and before every command that reads buffers while a persistent "
               "map writes");
}

}
