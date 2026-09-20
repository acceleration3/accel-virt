#include "proto/resources/shared_object_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

SharedObjectRecord::SharedObjectRecord() : object_id_(0u), kind_(), adapter_id_(0u), size_(0u)
{
    AV_SCOPE("SharedObjectRecord::SharedObjectRecord");
}

SharedObjectRecord::SharedObjectRecord(uint64_t object_id, const SharedObjectKind &kind, uint64_t adapter_id,
                                       uint64_t size)
    : object_id_(object_id), kind_(kind), adapter_id_(adapter_id), size_(size)
    {
        AV_SCOPE("SharedObjectRecord::SharedObjectRecord");
    }

uint64_t SharedObjectRecord::object_id() const
{
    AV_SCOPE("SharedObjectRecord::object_id");
    return object_id_;
}

const SharedObjectKind &SharedObjectRecord::kind() const
{
    AV_SCOPE("SharedObjectRecord::kind");
    return kind_;
}

uint64_t SharedObjectRecord::adapter_id() const
{
    AV_SCOPE("SharedObjectRecord::adapter_id");
    return adapter_id_;
}

uint64_t SharedObjectRecord::size() const
{
    AV_SCOPE("SharedObjectRecord::size");
    return size_;
}

bool SharedObjectRecord::assigned() const
{
    AV_SCOPE("SharedObjectRecord::assigned");
    return object_id_ != 0u && adapter_id_ != 0u;
}

void SharedObjectRecord::assign(uint64_t object_id, uint64_t adapter_id)
{
    AV_SCOPE("SharedObjectRecord::assign");
    object_id_ = object_id;
    adapter_id_ = adapter_id;
}

Status SharedObjectRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("SharedObjectRecord::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(version);
    writer.write_u64(object_id_);
    writer.write_u32(kind_.value());
    writer.write_zeros(4u);
    writer.write_u64(adapter_id_);
    writer.write_u64(size_);
    return writer.status();
}

bool SharedObjectRecord::fields_valid() const
{
    AV_SCOPE("SharedObjectRecord::fields_valid");
    if (!kind_.known()) return false;
    /* An id and an adapter travel together: a record naming one without the
     * other is one no miniport ever wrote. */
    if ((object_id_ == 0u) != (adapter_id_ == 0u)) return false;
    /* The same rule the wire body keeps, because the two describe one object
     * and an opener compares them: memory is whole non-zero pages, and a fence
     * has no size at all (feature 053, T031). */
    if (kind_.is_fence()) return size_ == 0u;
    return size_ != 0u && size_ % page_size == 0u;
}

Status SharedObjectRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("SharedObjectRecord::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t found_version = reader.read_u32();
    SharedObjectRecord candidate;
    candidate.object_id_ = reader.read_u64();
    candidate.kind_ = SharedObjectKind(reader.read_u32());
    const uint32_t reserved = reader.read_u32();
    candidate.adapter_id_ = reader.read_u64();
    candidate.size_ = reader.read_u64();
    if (found_magic != magic || found_version != version || reserved != 0u || !candidate.fields_valid())
        return Status::invalid_parameter;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
