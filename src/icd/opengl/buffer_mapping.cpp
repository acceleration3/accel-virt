#include "icd/opengl/buffer_mapping.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::glicd {

BufferMapping::BufferMapping() : dirty_from_(0u), dirty_to_(0u)
{
    AV_SCOPE("BufferMapping::BufferMapping");
}

BufferMapping::BufferMapping(uint32_t buffer, uint64_t offset, uint64_t length, uint32_t access)
    : StagedMapping(buffer, 0u, offset, length, access), dirty_from_(0u), dirty_to_(0u)
{
    AV_SCOPE("BufferMapping::BufferMapping");
}

uint32_t BufferMapping::buffer() const
{
    AV_SCOPE("BufferMapping::buffer");
    return static_cast<uint32_t>(object());
}

uint32_t BufferMapping::access() const
{
    AV_SCOPE("BufferMapping::access");
    return flags();
}

bool BufferMapping::persistent_writes() const
{
    AV_SCOPE("BufferMapping::persistent_writes");
    return (flags() & (map_persistent | map_write)) == (map_persistent | map_write);
}

Status BufferMapping::mark(uint64_t offset, uint64_t length)
{
    AV_SCOPE("BufferMapping::mark");
    if ((flags() & (map_write | map_flush_explicit)) != (map_write | map_flush_explicit))
        return Status::invalid_state;
    if (offset > bytes() || length > bytes() - offset) return Status::invalid_parameter;
    if (length == 0u) return Status::ok;
    const uint64_t end = offset + length;
    const bool     clean = dirty_from_ == dirty_to_;
    dirty_from_ = clean || offset < dirty_from_ ? offset : dirty_from_;
    dirty_to_ = clean || end > dirty_to_ ? end : dirty_to_;
    return Status::ok;
}

void BufferMapping::take_marked(uint64_t &from, uint64_t &to)
{
    AV_SCOPE("BufferMapping::take_marked");
    from = dirty_from_;
    to = dirty_to_;
    dirty_from_ = 0u;
    dirty_to_ = 0u;
}

void BufferMapping::report(StateWriter &writer, uint32_t index) const
{
    AV_SCOPE("BufferMapping::report");
    writer.open("mapping", index);
    writer.number("buffer", buffer());
    writer.number("offset", offset());
    writer.number("length", bytes());
    writer.hex("access", flags());
    writer.number("dirty_from", dirty_from_);
    writer.number("dirty_to", dirty_to_);
    writer.close();
}

bool BufferMapping::invariants_hold() const
{
    AV_SCOPE("BufferMapping::invariants_hold");
    if (!StagedMapping::invariants_hold()) return false;
    if (dirty_from_ > dirty_to_ || dirty_to_ > bytes()) return false;
    const bool marks = (flags() & (map_write | map_flush_explicit)) == (map_write | map_flush_explicit);
    return marks || dirty_to_ == 0u;
}

}
