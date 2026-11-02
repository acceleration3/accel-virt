#include "proto/resources/shared_allocation_data.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr size_t record_offset = AllocationDescriptor::encoded_size;

}

SharedAllocationData::SharedAllocationData() : bytes_{}, descriptor_(), record_(), read_(false)
{
    AV_SCOPE("SharedAllocationData::SharedAllocationData");
}

SharedAllocationData::SharedAllocationData(const AllocationDescriptor &descriptor, const SharedObjectKind &kind,
                                           uint64_t size)
    : bytes_{}, descriptor_(descriptor), record_(0u, kind, 0u, size), read_(true)
{
    AV_SCOPE("SharedAllocationData::SharedAllocationData");
    static_cast<void>(descriptor_.encode(bytes_, AllocationDescriptor::encoded_size));
    static_cast<void>(record_.encode(bytes_ + record_offset, SharedObjectRecord::encoded_size));
}

const uint8_t *SharedAllocationData::bytes() const
{
    AV_SCOPE("SharedAllocationData::bytes");
    return bytes_;
}

uint8_t *SharedAllocationData::lend()
{
    AV_SCOPE("SharedAllocationData::lend");
    read_ = false;
    return bytes_;
}

Status SharedAllocationData::reread(size_t written)
{
    AV_SCOPE("SharedAllocationData::reread");
    read_ = false;
    if (written != encoded_size) return Status::malformed;
    AllocationDescriptor descriptor;
    SharedObjectRecord   record;
    const Status         described = descriptor.decode(bytes_, AllocationDescriptor::encoded_size);
    if (described != Status::ok) return described;
    const Status recorded = record.decode(bytes_ + record_offset, SharedObjectRecord::encoded_size);
    if (recorded != Status::ok) return recorded;
    descriptor_ = descriptor;
    record_ = record;
    read_ = true;
    return Status::ok;
}

Status SharedAllocationData::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("SharedAllocationData::decode");
    if (bytes == nullptr || length != encoded_size) return Status::malformed;
    SharedAllocationData copied;
    __builtin_memcpy(copied.bytes_, bytes, encoded_size);
    const Status judged = copied.reread(length);
    if (judged != Status::ok) return judged;
    *this = copied;
    return Status::ok;
}

Status SharedAllocationData::mint(const SharedObjectRecord &minted)
{
    AV_SCOPE("SharedAllocationData::mint");
    if (!read_) return Status::invalid_state;
    const Status encoded = minted.encode(bytes_ + record_offset, SharedObjectRecord::encoded_size);
    if (encoded != Status::ok) return encoded;
    record_ = minted;
    return Status::ok;
}

bool SharedAllocationData::read() const
{
    AV_SCOPE("SharedAllocationData::read");
    return read_;
}

const AllocationDescriptor &SharedAllocationData::descriptor() const
{
    AV_SCOPE("SharedAllocationData::descriptor");
    return descriptor_;
}

const SharedObjectRecord &SharedAllocationData::record() const
{
    AV_SCOPE("SharedAllocationData::record");
    return record_;
}

bool SharedAllocationData::assigned() const
{
    AV_SCOPE("SharedAllocationData::assigned");
    return read_ && record_.assigned();
}

uint64_t SharedAllocationData::object_id() const
{
    AV_SCOPE("SharedAllocationData::object_id");
    return assigned() ? record_.object_id() : 0u;
}

uint64_t SharedAllocationData::size() const
{
    AV_SCOPE("SharedAllocationData::size");
    return read_ ? descriptor_.size() : 0u;
}

bool SharedAllocationData::invariants_hold() const
{
    AV_SCOPE("SharedAllocationData::invariants_hold");
    if (!read_) return true;
    uint8_t again[encoded_size] = {};
    if (descriptor_.encode(again, AllocationDescriptor::encoded_size) != Status::ok ||
        record_.encode(again + record_offset, SharedObjectRecord::encoded_size) != Status::ok)
        return false;
    for (size_t at = 0u; at < encoded_size; at++)
        if (again[at] != bytes_[at]) return false;
    return true;
}

}
