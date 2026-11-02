#include "device/memory/guest_page_mapping.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_window.hpp"
#include "device/host/host.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

GuestPageMapping::GuestPageMapping(Host &host)
    : host_(host), pointers_(nullptr), lengths_(nullptr), capacity_(0u), count_(0u), length_(0u)
    {
        AV_SCOPE("GuestPageMapping::GuestPageMapping");
    }

GuestPageMapping::~GuestPageMapping()
{
    AV_SCOPE("GuestPageMapping::~GuestPageMapping");
    release();
}

Status GuestPageMapping::map(ControlRequest &request, size_t offset, uint32_t entries)
{
    AV_SCOPE("GuestPageMapping::map");
    host_.require_main_loop("GuestPageMapping::map");
    if (pointers_ != nullptr) return Status::invalid_state;
    if (entries == 0u || entries > max_entries) return Status::invalid_parameter;
    Status status = reserve(entries);
    for (uint32_t index = 0u; index < entries && status == Status::ok; index++) {
        uint8_t    entry[entry_size] = {};
        const bool whole = request.read(offset + index * entry_size, entry, sizeof entry) == sizeof entry;
        WireReader reader(entry, sizeof entry);
        const uint64_t address = reader.read_u64();
        const uint64_t length = reader.read_u32();
        status = whole ? map_entry(address, length) : Status::invalid_parameter;
    }
    if (status != Status::ok) release();
    return status;
}

Status GuestPageMapping::map_entry(uint64_t address, uint64_t length)
{
    AV_SCOPE("GuestPageMapping::map_entry");
    if (length == 0u || address + length < address) return Status::invalid_parameter;
    while (length != 0u) {
        uint64_t mapped = length;
        bool     direct = false;
        void *const pointer = host_.map_guest(address, mapped, direct);
        if (pointer == nullptr || mapped == 0u) return Status::not_found;
        const Status status = direct ? add_extent(pointer, mapped) : Status::not_supported;
        if (status != Status::ok) {
            host_.unmap_guest(pointer, mapped);
            return status;
        }
        address += mapped;
        length -= mapped;
    }
    return Status::ok;
}

Status GuestPageMapping::reserve(uint32_t capacity)
{
    AV_SCOPE("GuestPageMapping::reserve");
    void *const block = host_.allocate(capacity * (sizeof(void *) + sizeof(uint64_t)), Pool::paged);
    if (block == nullptr) return Status::no_memory;
    auto *const pointers = static_cast<void **>(block);
    auto *const lengths = static_cast<uint64_t *>(static_cast<void *>(pointers + capacity));
    for (uint32_t index = 0u; index < count_; index++) {
        pointers[index] = pointers_[index];
        lengths[index] = lengths_[index];
    }
    if (pointers_ != nullptr) host_.release(pointers_);
    pointers_ = pointers;
    lengths_ = lengths;
    capacity_ = capacity;
    return Status::ok;
}

Status GuestPageMapping::add_extent(void *pointer, uint64_t length)
{
    AV_SCOPE("GuestPageMapping::add_extent");
    const Status status = count_ < capacity_ ? Status::ok : reserve(capacity_ * 2u);
    if (status != Status::ok) return status;
    pointers_[count_] = pointer;
    lengths_[count_] = length;
    count_++;
    length_ += length;
    return Status::ok;
}

void GuestPageMapping::release()
{
    AV_SCOPE("GuestPageMapping::release");
    host_.require_main_loop("GuestPageMapping::release");
    for (uint32_t index = 0u; index < count_; index++) host_.unmap_guest(pointers_[index], lengths_[index]);
    if (pointers_ != nullptr) host_.release(pointers_);
    pointers_ = nullptr;
    lengths_ = nullptr;
    capacity_ = 0u;
    count_ = 0u;
    length_ = 0u;
}

uint64_t GuestPageMapping::length() const
{
    AV_SCOPE("GuestPageMapping::length");
    return length_;
}

uint32_t GuestPageMapping::extents() const
{
    AV_SCOPE("GuestPageMapping::extents");
    return count_;
}

BlobWindow GuestPageMapping::window() const
{
    AV_SCOPE("GuestPageMapping::window");
    return BlobWindow(pointers_, lengths_, count_, length_);
}

bool GuestPageMapping::invariants_hold() const
{
    AV_SCOPE("GuestPageMapping::invariants_hold");
    if (count_ > capacity_ || (pointers_ != nullptr) != (capacity_ != 0u)) return false;
    uint64_t sum = 0u;
    for (uint32_t index = 0u; index < count_; index++) sum += lengths_[index];
    return sum == length_;
}

}
