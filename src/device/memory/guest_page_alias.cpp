#include "device/memory/guest_page_alias.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"

namespace av {

GuestPageAlias::GuestPageAlias(Host &host) : host_(host), base_(nullptr), length_(0u), runs_(0u)
{
    AV_SCOPE("GuestPageAlias::GuestPageAlias");
}

GuestPageAlias::~GuestPageAlias()
{
    AV_SCOPE("GuestPageAlias::~GuestPageAlias");
    release();
}

/* Everything the host will be asked is judged first, so a refusal reserves
 * nothing and a blob the host cannot hold spends none of its mappings. */
Status GuestPageAlias::map(const BlobWindow &pages)
{
    AV_SCOPE("GuestPageAlias::map");
    if (base_ != nullptr) return Status::invalid_state;
    if (pages.extents() == 0u) return Status::malformed;
    uint32_t       runs = 0u;
    uint64_t       total = 0u;
    const uint8_t *end = nullptr;
    for (uint32_t index = 0u; index < pages.extents(); index++) {
        uint64_t            bytes = 0u;
        const uint8_t *const start = static_cast<const uint8_t *>(pages.extent(index, bytes));
        const uint64_t      address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(start));
        if (start == nullptr || bytes == 0u || (address | bytes) % page_size != 0u) return Status::malformed;
        runs += start == end ? 0u : 1u;
        end = start + bytes;
        total += bytes;
    }
    if (runs > max_runs) return Status::no_memory;
    void *base = nullptr;
    const Status reserved = host_.reserve_range(total, base);
    if (reserved != Status::ok) return reserved;
    base_ = base;
    length_ = total;
    const Status aliased = map_runs(pages);
    if (aliased != Status::ok) {
        release();
        return aliased;
    }
    return Status::ok;
}

Status GuestPageAlias::map_runs(const BlobWindow &pages)
{
    AV_SCOPE("GuestPageAlias::map_runs");
    uint64_t offset = 0u;
    for (uint32_t index = 0u; index < pages.extents();) {
        uint64_t    run_bytes = 0u;
        void *const run = pages.extent(index++, run_bytes);
        uint64_t    next = 0u;
        while (pages.extent(index, next) == static_cast<void *>(static_cast<uint8_t *>(run) + run_bytes)) {
            run_bytes += next;
            index++;
        }
        const Status status = host_.alias_guest_run(run, run_bytes, static_cast<uint8_t *>(base_) + offset);
        if (status != Status::ok) return status;
        offset += run_bytes;
        runs_++;
    }
    return Status::ok;
}

void GuestPageAlias::release()
{
    AV_SCOPE("GuestPageAlias::release");
    if (base_ != nullptr) host_.release_range(base_, length_);
    base_ = nullptr;
    length_ = 0u;
    runs_ = 0u;
}

bool GuestPageAlias::mapped() const
{
    AV_SCOPE("GuestPageAlias::mapped");
    return base_ != nullptr;
}

void *GuestPageAlias::base() const
{
    AV_SCOPE("GuestPageAlias::base");
    return base_;
}

uint64_t GuestPageAlias::length() const
{
    AV_SCOPE("GuestPageAlias::length");
    return length_;
}

uint32_t GuestPageAlias::runs() const
{
    AV_SCOPE("GuestPageAlias::runs");
    return runs_;
}

/* A range is whole pages and held exactly while it has a base, and no run is
 * shorter than a page. */
bool GuestPageAlias::invariants_hold() const
{
    AV_SCOPE("GuestPageAlias::invariants_hold");
    if ((base_ == nullptr) != (length_ == 0u) || length_ % page_size != 0u) return false;
    return runs_ <= max_runs && uint64_t{runs_} * page_size <= length_;
}

}  // namespace av
