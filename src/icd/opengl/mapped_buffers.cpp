#include "icd/opengl/mapped_buffers.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/support/log_sink.hpp"
#include "icd/opengl/buffer_shadows.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

namespace {

constexpr uint32_t known_access = 0x007Fu;

/* What OpenGL (§6.3.1) says of the access bits alone. */
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

}  // namespace

MappedBuffers::MappedBuffers(BlockSource &memory, BufferShadows &shadows, LogSink &log)
    : memory_(memory), shadows_(shadows), log_(log), mappings_{}, persistent_writers_(0u), logged_()
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
    const uint32_t refused = access_error(access);
    if (refused != gl_no_error) return refused;
    if (offset < 0 || length == 0 || length < whole_buffer) return gl_invalid_value;
    if (buffer == 0u || find(buffer) != max_mappings) return gl_invalid_operation;
    uint64_t known = 0u;
    int64_t  size = -1;
    if (shadows_.size_of(buffer, known)) {
        size = static_cast<int64_t>(known);
    } else {
        glproto::av_glGetNamedBufferParameteri64v(wire, buffer, gl_buffer_size, &size);
        /* No answer: the host's own error says why, and the name is no buffer. */
        if (size < 0) return gl_invalid_operation;
        shadows_.sized(buffer, static_cast<uint64_t>(size));
    }
    const int64_t mapped = length == whole_buffer ? size : length;
    if (offset > size || mapped > size - offset) return gl_invalid_value;
    const uint32_t slot = find(0u);
    if (slot == max_mappings) return gl_out_of_memory;
    static_cast<void>(mappings_[slot].construct(memory_, shadows_));
    BufferMapping &mapping = mappings_[slot].object();
    const Status   made = mapping.map(wire, buffer, static_cast<uint64_t>(offset), static_cast<uint64_t>(mapped),
                                      access);
    if (made != Status::ok) {
        mappings_[slot].destroy();
        /* A buffer of no bytes has no range to map; anything else refused is
         * memory. */
        return mapped == 0 ? gl_invalid_value : gl_out_of_memory;
    }
    log_once();
    if (mapping.persistent_writes()) persistent_writers_++;
    pointer = mapping.pointer();
    return gl_no_error;
}

uint32_t MappedBuffers::unmap(ClientTransport &wire, uint32_t buffer)
{
    AV_SCOPE("MappedBuffers::unmap");
    const uint32_t slot = buffer != 0u ? find(buffer) : max_mappings;
    if (slot == max_mappings) return gl_invalid_operation;
    mappings_[slot].object().write_back(wire);
    drop(slot);
    return gl_no_error;
}

uint32_t MappedBuffers::mark(uint32_t buffer, int64_t offset, int64_t length)
{
    AV_SCOPE("MappedBuffers::mark");
    const uint32_t slot = buffer != 0u ? find(buffer) : max_mappings;
    if (offset < 0 || length < 0) return gl_invalid_value;
    if (slot == max_mappings) return gl_invalid_operation;
    const Status marked =
        mappings_[slot].object().mark(static_cast<uint64_t>(offset), static_cast<uint64_t>(length));
    if (marked == Status::invalid_state) return gl_invalid_operation;
    return marked == Status::ok ? gl_no_error : gl_invalid_value;
}

void *MappedBuffers::pointer_of(uint32_t buffer) const
{
    AV_SCOPE("MappedBuffers::pointer_of");
    const uint32_t slot = buffer != 0u ? find(buffer) : max_mappings;
    return slot != max_mappings ? mappings_[slot].object().pointer() : nullptr;
}

bool MappedBuffers::map_state(uint32_t buffer, uint32_t pname, int64_t &value) const
{
    AV_SCOPE("MappedBuffers::map_state");
    const uint32_t       slot = buffer != 0u ? find(buffer) : max_mappings;
    const BufferMapping *mapping = slot != max_mappings ? &mappings_[slot].object() : nullptr;
    const uint32_t       access = mapping != nullptr ? mapping->access() : 0u;
    const uint32_t       both = BufferMapping::map_read | BufferMapping::map_write;
    switch (pname) {
    case gl_buffer_mapped:
        value = mapping != nullptr ? 1 : 0;
        return true;
    case gl_buffer_access_flags:
        value = access;
        return true;
    case gl_buffer_map_offset:
        value = mapping != nullptr ? static_cast<int64_t>(mapping->offset()) : 0;
        return true;
    case gl_buffer_map_length:
        value = mapping != nullptr ? static_cast<int64_t>(mapping->length()) : 0;
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
    for (ObjectStorage<BufferMapping> &mapping : mappings_) {
        if (mapping.constructed() && mapping.object().persistent_writes()) mapping.object().write_back(wire);
    }
}

void MappedBuffers::forget(int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("MappedBuffers::forget");
    if (buffers == nullptr) return;
    for (int32_t at = 0; at < count; at++) {
        const uint32_t slot = buffers[at] != 0u ? find(buffers[at]) : max_mappings;
        if (slot != max_mappings) drop(slot);
    }
}

void MappedBuffers::release()
{
    AV_SCOPE("MappedBuffers::release");
    for (uint32_t slot = 0u; slot < max_mappings; slot++) {
        if (mappings_[slot].constructed()) drop(slot);
    }
}

uint32_t MappedBuffers::held() const
{
    AV_SCOPE("MappedBuffers::held");
    uint32_t count = 0u;
    for (const ObjectStorage<BufferMapping> &mapping : mappings_) count += mapping.constructed() ? 1u : 0u;
    return count;
}

void MappedBuffers::report(StateWriter &writer) const
{
    AV_SCOPE("MappedBuffers::report");
    writer.open("mappings", 0u);
    writer.number("held", held());
    writer.number("persistent_writers", persistent_writers_);
    for (uint32_t slot = 0u; slot < max_mappings; slot++) {
        if (mappings_[slot].constructed()) mappings_[slot].object().report(writer, slot);
    }
    writer.close();
}

/* Each mapping is whole and maps a buffer no other one does, and the
 * persistent writers are counted. */
bool MappedBuffers::invariants_hold() const
{
    AV_SCOPE("MappedBuffers::invariants_hold");
    uint32_t writers = 0u;
    for (uint32_t slot = 0u; slot < max_mappings; slot++) {
        if (!mappings_[slot].constructed()) continue;
        const BufferMapping &mapping = mappings_[slot].object();
        if (!mapping.invariants_hold() || mapping.buffer() == 0u || find(mapping.buffer()) != slot) return false;
        writers += mapping.persistent_writes() ? 1u : 0u;
    }
    return writers == persistent_writers_;
}

uint32_t MappedBuffers::find(uint32_t buffer) const
{
    AV_SCOPE("MappedBuffers::find");
    for (uint32_t slot = 0u; slot < max_mappings; slot++) {
        const bool held = mappings_[slot].constructed();
        if (buffer == 0u ? !held : held && mappings_[slot].object().buffer() == buffer) return slot;
    }
    return max_mappings;
}

void MappedBuffers::drop(uint32_t slot)
{
    AV_SCOPE("MappedBuffers::drop");
    if (mappings_[slot].object().persistent_writes()) persistent_writers_--;
    mappings_[slot].destroy();
}

void MappedBuffers::log_once()
{
    AV_SCOPE("MappedBuffers::log_once");
    if (!logged_.first()) return;
    log_.write("avgl: buffer maps are guest copies: filled at map from what the host holds, the bytes that "
               "changed written back at unmap, and before every command that reads buffers while a persistent "
               "map writes");
}

}  // namespace av::glicd
