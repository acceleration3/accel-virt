#include "device/memory/guest_page_overlay.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/host/host.hpp"
#include "device/memory/lent_pages.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

namespace {

constexpr const char *unrestored = "virtio-gpu-av: a lent guest run could not be given its RAM back";

}  // namespace

GuestPageOverlay::GuestPageOverlay(Host &host, LentPages &lent)
    : host_(host), lent_(lent), runs_(host, Pool::paged), covered_(0u)
{
    AV_SCOPE("GuestPageOverlay::GuestPageOverlay");
}

GuestPageOverlay::~GuestPageOverlay()
{
    AV_SCOPE("GuestPageOverlay::~GuestPageOverlay");
    restore();
}

Status GuestPageOverlay::add(ControlRequest &request, size_t offset, uint32_t entries, int fd, uint64_t size)
{
    AV_SCOPE("GuestPageOverlay::add");
    host_.require_main_loop("GuestPageOverlay::add");
    /* A list shorter than it says is refused before any of it is laid. */
    uint8_t entry[entry_size] = {};
    if (entries == 0u || request.read(offset + size_t{entries - 1u} * entry_size, entry, sizeof entry) != sizeof entry)
        return Status::invalid_parameter;
    for (uint32_t index = 0u; index < entries; index++) {
        static_cast<void>(request.read(offset + index * entry_size, entry, sizeof entry));
        WireReader     reader(entry, sizeof entry);
        const uint64_t address = reader.read_u64();
        const uint64_t length = reader.read_u32();
        const Status   laid = lay_entry(address, length, fd, size);
        if (laid != Status::ok) return laid;
    }
    return Status::ok;
}

/* An entry may cross a region boundary, which QEMU maps as two runs. */
Status GuestPageOverlay::lay_entry(uint64_t address, uint64_t length, int fd, uint64_t size)
{
    AV_SCOPE("GuestPageOverlay::lay_entry");
    if (length == 0u || address + length < address || address % page_size != 0u || length % page_size != 0u ||
        length > size - covered_)
        return Status::invalid_parameter;
    const Status claimed = lent_.claim(address, length);
    if (claimed != Status::ok) return claimed;
    while (length != 0u) {
        uint64_t    mapped = length;
        bool        direct = false;
        void *const pointer = host_.map_guest(address, mapped, direct);
        Status      status = pointer == nullptr || mapped == 0u ? Status::not_found : Status::ok;
        if (status == Status::ok) status = direct ? lay_run(pointer, mapped, address, fd) : Status::not_supported;
        if (status != Status::ok) {
            if (pointer != nullptr) host_.unmap_guest(pointer, mapped);
            lent_.release(address, length);
            return status;
        }
        address += mapped;
        length -= mapped;
    }
    return Status::ok;
}

/* Room for the record is made first: a run laid and not recorded could never
 * be restored. */
Status GuestPageOverlay::lay_run(void *pointer, uint64_t length, uint64_t address, int fd)
{
    AV_SCOPE("GuestPageOverlay::lay_run");
    if (runs_.reserve(runs_.size() + 1u) != Status::ok) return Status::no_memory;
    const Status overlaid = host_.overlay_guest_run(pointer, length, fd, covered_);
    if (overlaid != Status::ok) return overlaid;
    static_cast<void>(runs_.append(Run{pointer, length, address}));
    covered_ += length;
    return Status::ok;
}

void GuestPageOverlay::restore()
{
    AV_SCOPE("GuestPageOverlay::restore");
    for (uint32_t index = runs_.size(); index-- != 0u;) {
        const Run &run = runs_.at(index);
        if (host_.restore_guest_run(run.pointer_, run.length_) != Status::ok) host_.log(unrestored);
        host_.unmap_guest(run.pointer_, run.length_);
        lent_.release(run.address_, run.length_);
    }
    runs_.clear();
    covered_ = 0u;
}

uint64_t GuestPageOverlay::covered() const
{
    AV_SCOPE("GuestPageOverlay::covered");
    return covered_;
}

uint32_t GuestPageOverlay::runs() const
{
    AV_SCOPE("GuestPageOverlay::runs");
    return runs_.size();
}

/* The bytes laid are the runs' sum, and every laid page is lent. */
bool GuestPageOverlay::invariants_hold() const
{
    AV_SCOPE("GuestPageOverlay::invariants_hold");
    uint64_t sum = 0u;
    for (const Run &run : runs_) {
        sum += run.length_;
        if (!lent_.lent(run.address_)) return false;
    }
    return runs_.invariants_hold() && sum == covered_;
}

}  // namespace av
