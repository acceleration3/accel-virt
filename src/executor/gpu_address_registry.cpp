#include "executor/gpu_address_registry.hpp"

#include "core/support/sorted_search.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

GpuAddressRegistry::GpuAddressRegistry(BlockSource &blocks) : rows_(blocks, Pool::paged)
{
    AV_SCOPE("GpuAddressRegistry::GpuAddressRegistry");
}

uint32_t GpuAddressRegistry::row_holding(uint64_t address) const
{
    AV_SCOPE("GpuAddressRegistry::row_holding");
    const SortedSearch<Row, &Row::base_> search(rows_);
    const uint32_t                       row = search.at_or_below(address);
    if (row == rows_.size()) return row;
    const Row &found = rows_.at(row);
    return address - found.base_ < found.bytes_ ? row : rows_.size();
}

Status GpuAddressRegistry::learn(uint64_t device, uint64_t owner, uint64_t address, uint64_t bytes)
{
    AV_SCOPE("GpuAddressRegistry::learn");
    if (address == 0u || bytes == 0u || address + bytes < address) return Status::invalid_parameter;
    const SortedSearch<Row, &Row::base_> search(rows_);
    const uint32_t                       at = search.at_or_after(address);
    if (at < rows_.size() && rows_.at(at).base_ == address) {
        Row &known = rows_.at(at);
        if (known.owner_ != owner || known.device_ != device) return Status::invalid_state;
        known.bytes_ = bytes;
        return Status::ok;
    }
    if (at > 0u) {
        const Row &before = rows_.at(at - 1u);
        if (address - before.base_ < before.bytes_) return Status::invalid_state;
    }
    if (at < rows_.size() && rows_.at(at).base_ - address < bytes) return Status::invalid_state;
    return rows_.insert_at(at, Row{address, bytes, owner, device});
}

void GpuAddressRegistry::forget_owner(uint64_t owner)
{
    AV_SCOPE("GpuAddressRegistry::forget_owner");
    for (uint32_t at = 0u; at < rows_.size();) {
        if (rows_.at(at).owner_ == owner) rows_.remove_at(at);
        else at++;
    }
}

void GpuAddressRegistry::forget_device(uint64_t device)
{
    AV_SCOPE("GpuAddressRegistry::forget_device");
    for (uint32_t at = 0u; at < rows_.size();) {
        if (rows_.at(at).device_ == device) rows_.remove_at(at);
        else at++;
    }
}

bool GpuAddressRegistry::covers(uint64_t device, uint64_t address, uint64_t bytes) const
{
    AV_SCOPE("GpuAddressRegistry::covers");
    const uint32_t row = row_holding(address);
    if (row == rows_.size()) return false;
    const Row &found = rows_.at(row);
    return found.device_ == device && bytes <= found.bytes_ - (address - found.base_);
}

uint64_t GpuAddressRegistry::room_after(uint64_t device, uint64_t address) const
{
    AV_SCOPE("GpuAddressRegistry::room_after");
    const uint32_t row = row_holding(address);
    if (row == rows_.size() || rows_.at(row).device_ != device) return 0u;
    return rows_.at(row).bytes_ - (address - rows_.at(row).base_);
}

uint32_t GpuAddressRegistry::count() const
{
    AV_SCOPE("GpuAddressRegistry::count");
    return rows_.size();
}

bool GpuAddressRegistry::invariants_hold() const
{
    AV_SCOPE("GpuAddressRegistry::invariants_hold");
    const SortedSearch<Row, &Row::base_> search(rows_);
    if (!search.invariants_hold()) return false;
    for (uint32_t at = 1u; at < rows_.size(); at++) {
        const Row &before = rows_.at(at - 1u);
        if (rows_.at(at).base_ - before.base_ < before.bytes_) return false;
    }
    return true;
}

}
