#include "device/memory/guest_ram_lending.hpp"

#include "core/trace/function_scope.hpp"
#include "device/control/control_request.hpp"
#include "device/host/host.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

GuestRamLending::GuestRamLending(BlockSource &blocks, LentPages &pages, RunSwapGate &gate, LendingLife &life,
                                 uint64_t id, uint64_t size, int fd, ContextPlane &plane, uint64_t key)
    : life_(life), id_(id), size_(size), fd_(fd), overlay_(blocks, pages, gate, *this), holders_(blocks, Pool::paged)
{
    AV_SCOPE("GuestRamLending::GuestRamLending");
    static_cast<void>(holders_.append(Holder{&plane, key}));
}

GuestRamLending::~GuestRamLending()
{
    AV_SCOPE("GuestRamLending::~GuestRamLending");
}

uint64_t GuestRamLending::id() const
{
    AV_SCOPE("GuestRamLending::id");
    return id_;
}

LendingLife &GuestRamLending::life() const
{
    AV_SCOPE("GuestRamLending::life");
    return life_;
}

int GuestRamLending::descriptor() const
{
    AV_SCOPE("GuestRamLending::descriptor");
    return fd_;
}

bool GuestRamLending::locate(uint64_t address, uint64_t length, uint32_t &hint, uint64_t &offset) const
{
    AV_SCOPE("GuestRamLending::locate");
    return overlay_.locate(address, length, hint, offset);
}

uint64_t GuestRamLending::size() const
{
    AV_SCOPE("GuestRamLending::size");
    return size_;
}

uint64_t GuestRamLending::start() const
{
    AV_SCOPE("GuestRamLending::start");
    return overlay_.extents() != 0u ? overlay_.extent_address(0u) : 0u;
}

bool GuestRamLending::complete() const
{
    AV_SCOPE("GuestRamLending::complete");
    return overlay_.covered() == size_;
}

Status GuestRamLending::lay(Host &host, ControlRequest &request, size_t offset, uint32_t entries)
{
    AV_SCOPE("GuestRamLending::lay");
    return overlay_.add(host, request, offset, entries, fd_, size_);
}

Status GuestRamLending::verify(ControlRequest &request, size_t offset, uint32_t entries, uint32_t &next) const
{
    AV_SCOPE("GuestRamLending::verify");
    constexpr size_t entry_size = GuestPageOverlay::entry_size;
    uint8_t          entry[entry_size] = {};
    if (entries == 0u || request.read(offset + size_t{entries - 1u} * entry_size, entry, sizeof entry) != sizeof entry)
        return Status::invalid_parameter;
    if (entries > overlay_.extents() - next) return Status::invalid_parameter;
    for (uint32_t index = 0u; index < entries; index++) {
        static_cast<void>(request.read(offset + index * entry_size, entry, sizeof entry));
        WireReader     reader(entry, sizeof entry);
        const uint64_t address = reader.read_u64();
        const uint64_t length = reader.read_u32();
        if (address != overlay_.extent_address(next + index) || length != overlay_.extent_length(next + index))
            return Status::invalid_parameter;
    }
    next += entries;
    return Status::ok;
}

Status GuestRamLending::join(ContextPlane &plane, uint64_t key)
{
    AV_SCOPE("GuestRamLending::join");
    return holders_.append(Holder{&plane, key});
}

uint32_t GuestRamLending::leave(const ContextPlane &plane, uint64_t key)
{
    AV_SCOPE("GuestRamLending::leave");
    for (uint32_t index = 0u; index < holders_.size(); index++) {
        if (holders_.at(index).plane_ != &plane || holders_.at(index).key_ != key) continue;
        holders_.remove_swap(index);
        break;
    }
    return holders_.size();
}

bool GuestRamLending::holder_at(uint32_t index, ContextPlane *&plane, uint64_t &key) const
{
    AV_SCOPE("GuestRamLending::holder_at");
    if (index >= holders_.size()) return false;
    plane = holders_.at(index).plane_;
    key = holders_.at(index).key_;
    return true;
}

bool GuestRamLending::held_beyond(const ContextPlane &plane) const
{
    AV_SCOPE("GuestRamLending::held_beyond");
    for (const Holder &holder : holders_)
        if (holder.plane_ != &plane) return true;
    return false;
}

uint32_t GuestRamLending::holders() const
{
    AV_SCOPE("GuestRamLending::holders");
    return holders_.size();
}

void GuestRamLending::give_back(Host &host)
{
    AV_SCOPE("GuestRamLending::give_back");
    overlay_.restore(host);
    if (fd_ >= 0) host.close_descriptor(fd_);
    fd_ = -1;
}

/* A lending that is held has its memory; the pages it lies over are the
 * overlay's, and none are laid past its size. */
bool GuestRamLending::invariants_hold() const
{
    AV_SCOPE("GuestRamLending::invariants_hold");
    return holders_.invariants_hold() && overlay_.invariants_hold() && overlay_.covered() <= size_ &&
           (holders_.size() == 0u || fd_ >= 0);
}

}  // namespace av
