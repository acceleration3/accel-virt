#include "executor/host_memory_source.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/report_gate.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "executor/memory_exporter.hpp"
#include "executor/owned_fd.hpp"

namespace av {

static_assert(GuestMemoryTypes::no_type == HostMemoryRequest::memory_types);

HostMemorySource::HostMemorySource(MemoryExporter &exporter, BlockSource &blocks)
    : exporter_(exporter), memories_(blocks), aliases_(blocks), placed_(blocks), placed_sequence_(0u), lock_(), host_(), asked_(0u), mappable_(0u),
      visible_(0u), vulkan_(), vram_(), host_ram_(), device_ram_(), reuse_()
{
    AV_SCOPE("HostMemorySource::HostMemorySource");
}

HostMemorySource::~HostMemorySource()
{
    AV_SCOPE("HostMemorySource::~HostMemorySource");
    memories_.for_each([this](const SourceMemory &memory) { exporter_.free(memory.exported()); });
    memories_.destroy_all();
    static_cast<void>(host_ram_.close());
    static_cast<void>(device_ram_.close());
}

void HostMemorySource::learn()
{
    AV_SCOPE("HostMemorySource::learn");
    exporter_.memory_properties(host_);
    if (host_.memoryTypeCount > HostMemoryRequest::memory_types) host_.memoryTypeCount = HostMemoryRequest::memory_types;
    const uint32_t before = asked_;
    for (uint32_t type = 0u; type < host_.memoryTypeCount; type++) {
        const uint32_t bit = 1u << type;
        if ((asked_ & bit) != 0u) continue;
        asked_ |= bit;
        const VkMemoryPropertyFlags flags = host_.memoryTypes[type].propertyFlags;
        constexpr VkMemoryPropertyFlags shared = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if ((flags & shared) != shared || !exporter_.exportable(type)) continue;
        mappable_ |= bit;
        if (exporter_.imports_host_visible(type)) visible_ |= bit;
    }
    if (asked_ != before) vulkan_.learn(host_, mappable_);
}

uint32_t HostMemorySource::mappable_types()
{
    AV_SCOPE("HostMemorySource::mappable_types");
    const ThreadLockHold held(lock_);
    learn();
    return mappable_;
}

bool HostMemorySource::vulkan_types(GuestMemoryTypes &answer)
{
    AV_SCOPE("HostMemorySource::vulkan_types");
    const ThreadLockHold held(lock_);
    learn();
    if (asked_ == 0u) return false;
    answer = vulkan_;
    return true;
}

bool HostMemorySource::system_type(uint32_t type) const
{
    AV_SCOPE("HostMemorySource::system_type");
    if (type >= host_.memoryTypeCount) return true;
    const uint32_t heap = host_.memoryTypes[type].heapIndex;
    return heap >= host_.memoryHeapCount || (host_.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0u;
}

uint32_t HostMemorySource::first_type(uint32_t among, VkMemoryPropertyFlags wanted, VkMemoryPropertyFlags refused) const
{
    AV_SCOPE("HostMemorySource::first_type");
    for (uint32_t type = 0u; type < HostMemoryRequest::memory_types; type++) {
        if ((among & (1u << type)) == 0u) continue;
        const VkMemoryPropertyFlags flags = host_.memoryTypes[type].propertyFlags;
        if ((flags & wanted) == wanted && (flags & refused) == 0u) return type;
    }
    return HostMemoryRequest::memory_types;
}

uint32_t HostMemorySource::choices(HostMemoryRequest request, uint32_t (&types)[most_choices]) const
{
    AV_SCOPE("HostMemorySource::choices");
    uint32_t found[most_choices] = {};
    for (uint32_t &type : found) type = HostMemoryRequest::memory_types;
    constexpr uint32_t per_group = most_choices / 2u;
    const uint32_t     groups[2] = {visible_, mappable_ & ~visible_};
    constexpr VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    constexpr VkMemoryPropertyFlags cached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    if (request.kind() == HostMemoryRequest::Kind::vulkan) {
        found[0] = vulkan_.source_type(request.vulkan_type());
    } else if (request.kind() == HostMemoryRequest::Kind::upload) {
        for (uint32_t group = 0u; group < 2u; group++) {
            found[group * per_group] = first_type(groups[group], local, cached);
            found[group * per_group + 1u] = first_type(groups[group], 0u, local | cached);
            found[group * per_group + 2u] = first_type(groups[group], 0u, 0u);
        }
    } else {
        for (uint32_t group = 0u; group < 2u; group++) {
            found[group * per_group] = first_type(groups[group], cached, 0u);
            found[group * per_group + 1u] = first_type(groups[group], 0u, 0u);
        }
    }
    uint32_t count = 0u;
    for (const uint32_t type : found) {
        bool seen = type == HostMemoryRequest::memory_types;
        for (uint32_t earlier = 0u; earlier < count; earlier++) seen = seen || types[earlier] == type;
        if (!seen) types[count++] = type;
    }
    if (request.kind() == HostMemoryRequest::Kind::vulkan) return count;
    for (const uint32_t group : groups)
        for (uint32_t type = 0u; type < HostMemoryRequest::memory_types && count < most_choices; type++) {
            bool seen = (group & (1u << type)) == 0u || !system_type(type);
            for (uint32_t earlier = 0u; earlier < count; earlier++) seen = seen || types[earlier] == type;
            if (!seen) types[count++] = type;
        }
    return count;
}

Status HostMemorySource::allocate(uint32_t domain, uint64_t key, uint64_t size, HostMemoryRequest request,
                                  AvHostMemory &memory)
{
    AV_SCOPE("HostMemorySource::allocate");
    if (!request.valid() || size == 0u || size % page_bytes != 0u) return Status::invalid_parameter;
    const ThreadLockHold held(lock_);
    learn();
    if (find(domain, key) != nullptr || aliased(domain, key) != nullptr) return Status::conflict;
    uint32_t       types[most_choices] = {};
    const uint32_t count = choices(request, types);
    if (count == 0u) return Status::invalid_parameter;
    ExportedMemory exported;
    uint32_t       type = HostMemoryRequest::memory_types;
    uint64_t       charged = 0u;
    bool           charged_device = false;
    for (uint32_t choice = 0u; choice < count && type == HostMemoryRequest::memory_types; choice++) {
        const bool system = system_type(types[choice]);
        if (system && !host_ram_.take(size)) continue;
        if (!system && !device_ram_.take(size)) continue;
        if (exporter_.allocate(types[choice], size, exported) == Status::ok) {
            type = types[choice];
            charged = size;
            charged_device = !system;
        } else if (system) {
            host_ram_.give(size);
        } else {
            device_ram_.give(size);
        }
    }
    if (type == HostMemoryRequest::memory_types) return Status::no_memory;
    if (!system_type(type) && reuse_.crosses_domains(type, domain))
        __builtin_memset(exported.pointer(), 0, static_cast<size_t>(exported.size()));
    OwnedFd       shared;
    SourceMemory *made = nullptr;
    const uint32_t caching = (host_.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0u
                                 ? AV_MEMORY_WRITE_BACK
                                 : AV_MEMORY_WRITE_COMBINED;
    if (!shared.hold_copy(exported.fd()) ||
        memories_.create(made, domain, key, type, caching, exported) != Status::ok) {
        exporter_.free(exported);
        if (charged_device) device_ram_.give(charged);
        else host_ram_.give(charged);
        return Status::no_memory;
    }
    made->bind(*this);
    made->charge(charged, charged_device);
    memory.fd = shared.take();
    memory.caching = caching;
    memory.size = exported.size();
    return Status::ok;
}

Status HostMemorySource::alias(uint32_t domain, uint64_t key, HostMemorySource &from, uint32_t from_domain,
                               uint64_t from_key, AvHostMemory &memory)
{
    AV_SCOPE("HostMemorySource::alias");
    SourceMemory *const lent = from.acquire(from_domain, from_key);
    if (lent == nullptr) return Status::not_found;
    Status status = Status::ok;
    {
        const ThreadLockHold held(lock_);
        Alias *made = nullptr;
        if (find(domain, key) != nullptr || aliased(domain, key) != nullptr) status = Status::conflict;
        else if (aliases_.create(made, Alias{domain, key, lent}) != Status::ok) status = Status::no_memory;
    }
    if (status != Status::ok) {
        lent->owner()->release(*lent);
        return status;
    }
    memory = AvHostMemory{-1, lent->caching(), lent->size()};
    return Status::ok;
}

SourceMemory *HostMemorySource::aliased(uint32_t domain, uint64_t key)
{
    AV_SCOPE("HostMemorySource::aliased");
    const Alias *const alias =
        aliases_.find_if([domain, key](const Alias &held) { return held.domain_ == domain && held.key_ == key; });
    return alias != nullptr ? alias->memory_ : nullptr;
}

void HostMemorySource::release_aliases()
{
    AV_SCOPE("HostMemorySource::release_aliases");
    for (;;) {
        SourceMemory *lent = nullptr;
        {
            const ThreadLockHold held(lock_);
            Alias *const alias = aliases_.find_if([](const Alias &) { return true; });
            if (alias == nullptr) return;
            lent = alias->memory_;
            static_cast<void>(aliases_.destroy(alias));
        }
        lent->owner()->release(*lent);
    }
}

SourceMemory *HostMemorySource::find(uint32_t domain, uint64_t key)
{
    AV_SCOPE("HostMemorySource::find");
    return memories_.find_if([domain, key](const SourceMemory &memory) { return memory.answers(domain, key); });
}

void HostMemorySource::let_go(SourceMemory &memory)
{
    AV_SCOPE("HostMemorySource::let_go");
    if (!memory.release()) return;
    exporter_.free(memory.exported());
    if (!system_type(memory.memory_type())) reuse_.freed(memory.memory_type(), memory.domain());
    if (memory.charged_device()) device_ram_.give(memory.charged());
    else host_ram_.give(memory.charged());
    static_cast<void>(memories_.destroy(&memory));
}

void HostMemorySource::free(uint32_t domain, uint64_t key)
{
    AV_SCOPE("HostMemorySource::free");
    SourceMemory *lent = nullptr;
    {
        const ThreadLockHold held(lock_);
        SourceMemory *const memory = find(domain, key);
        if (memory != nullptr) {
            memory->detach();
            let_go(*memory);
            return;
        }
        Alias *const alias =
            aliases_.find_if([domain, key](const Alias &held_alias) { return held_alias.domain_ == domain && held_alias.key_ == key; });
        if (alias == nullptr) return;
        lent = alias->memory_;
        static_cast<void>(aliases_.destroy(alias));
    }
    lent->owner()->release(*lent);
}

void HostMemorySource::free_domain(uint32_t domain)
{
    AV_SCOPE("HostMemorySource::free_domain");
    {
        const ThreadLockHold held(lock_);
        for (SourceMemory *memory = nullptr;
             (memory = memories_.find_if([domain](const SourceMemory &held_memory) {
                  return held_memory.attached() && held_memory.domain() == domain;
              })) != nullptr;) {
            memory->detach();
            let_go(*memory);
        }
    }
    for (;;) {
        SourceMemory *lent = nullptr;
        {
            const ThreadLockHold held(lock_);
            Alias *const alias =
                aliases_.find_if([domain](const Alias &held_alias) { return held_alias.domain_ == domain; });
            if (alias == nullptr) return;
            lent = alias->memory_;
            static_cast<void>(aliases_.destroy(alias));
        }
        lent->owner()->release(*lent);
    }
}

uint32_t HostMemorySource::placed_of(uint64_t owner) const
{
    AV_SCOPE("HostMemorySource::placed_of");
    uint32_t count = 0u;
    placed_.for_each([owner, &count](const Placed &held) { count += held.owner_ == owner ? 1u : 0u; });
    return count;
}

void HostMemorySource::drop_oldest_placed(uint64_t owner)
{
    AV_SCOPE("HostMemorySource::drop_oldest_placed");
    Placed *oldest = nullptr;
    placed_.for_each([owner, &oldest](const Placed &held) {
        if (held.owner_ == owner && (oldest == nullptr || held.sequence_ < oldest->sequence_))
            oldest = const_cast<Placed *>(&held);
    });
    if (oldest != nullptr) static_cast<void>(placed_.destroy(oldest));
}

Status HostMemorySource::place(uint64_t owner, uint64_t handle, uint32_t type, uint64_t size, uint32_t caching, int fd)
{
    AV_SCOPE("HostMemorySource::place");
    if (owner == 0u || handle == 0u || size == 0u || type >= HostMemoryRequest::memory_types || fd < 0)
        return Status::invalid_parameter;
    const ThreadLockHold held(lock_);
    while (placed_of(owner) >= most_placed_per_owner) drop_oldest_placed(owner);
    Placed *made = nullptr;
    if (placed_.create(made, owner, handle, type, size, caching, placed_sequence_++) != Status::ok) return Status::no_memory;
    if (!made->fd_.hold_copy(fd)) {
        static_cast<void>(placed_.destroy(made));
        return Status::no_memory;
    }
    return Status::ok;
}

Status HostMemorySource::take_placed(uint64_t owner, uint32_t type, uint64_t size, AvHostMemory &memory)
{
    AV_SCOPE("HostMemorySource::take_placed");
    const ThreadLockHold held(lock_);
    Placed *found = nullptr;
    placed_.for_each([owner, type, size, &found](const Placed &candidate) {
        const uint64_t rounded = (candidate.size_ + page_bytes - 1u) / page_bytes * page_bytes;
        if (candidate.owner_ != owner || candidate.type_ != type || rounded != size) return;
        if (found == nullptr || candidate.sequence_ < found->sequence_) found = const_cast<Placed *>(&candidate);
    });
    if (found == nullptr) return Status::not_found;
    memory.fd = found->fd_.take();
    memory.caching = found->caching_;
    memory.size = size;
    static_cast<void>(placed_.destroy(found));
    return Status::ok;
}

void HostMemorySource::drop_placed(uint64_t owner, uint64_t handle)
{
    AV_SCOPE("HostMemorySource::drop_placed");
    const ThreadLockHold held(lock_);
    Placed *found = placed_.find_if([owner, handle](const Placed &candidate) {
        return candidate.owner_ == owner && candidate.handle_ == handle;
    });
    if (found != nullptr) static_cast<void>(placed_.destroy(found));
}

void HostMemorySource::drop_placed_of(uint64_t owner)
{
    AV_SCOPE("HostMemorySource::drop_placed_of");
    const ThreadLockHold held(lock_);
    while (placed_of(owner) != 0u) drop_oldest_placed(owner);
}

uint32_t HostMemorySource::placed() const
{
    AV_SCOPE("HostMemorySource::placed");
    return placed_.live();
}

SourceMemory *HostMemorySource::acquire(uint32_t domain, uint64_t key)
{
    AV_SCOPE("HostMemorySource::acquire");
    const ThreadLockHold held(lock_);
    SourceMemory *memory = find(domain, key);
    if (memory != nullptr) {
        memory->acquire();
        return memory;
    }
    memory = aliased(domain, key);
    if (memory != nullptr) memory->acquire();
    return memory;
}

void HostMemorySource::release(SourceMemory &memory)
{
    AV_SCOPE("HostMemorySource::release");
    HostMemorySource *const owner = memory.owner();
    if (owner != nullptr && owner != this) {
        owner->release(memory);
        return;
    }
    const ThreadLockHold held(lock_);
    let_go(memory);
}

void HostMemorySource::reserve_vram(uint32_t mib)
{
    AV_SCOPE("HostMemorySource::reserve_vram");
    const ThreadLockHold held(lock_);
    vram_ = VramPolicy(mib);
}

void HostMemorySource::use_vram_ledger(ByteLedger *ledger)
{
    AV_SCOPE("HostMemorySource::use_vram_ledger");
    const ThreadLockHold held(lock_);
    if (!device_ram_.bound_to(ledger)) device_ram_.bind(ledger);
}

void HostMemorySource::use_host_ram(ByteLedger *ledger)
{
    AV_SCOPE("HostMemorySource::use_host_ram");
    const ThreadLockHold held(lock_);
    if (!host_ram_.bound_to(ledger)) host_ram_.bind(ledger);
}

uint64_t HostMemorySource::host_ram_held() const
{
    AV_SCOPE("HostMemorySource::host_ram_held");
    return host_ram_.held();
}

VramPolicy HostMemorySource::vram_policy() const
{
    AV_SCOPE("HostMemorySource::vram_policy");
    const ThreadLockHold held(lock_);
    return vram_;
}

uint32_t HostMemorySource::live() const
{
    AV_SCOPE("HostMemorySource::live");
    return memories_.live();
}

void HostMemorySource::report(StateWriter &writer) const
{
    AV_SCOPE("HostMemorySource::report");
    const ReportGate gate(lock_, writer, "memory_source");
    if (!gate.held()) return;
    writer.hex("mappable_types", mappable_);
    writer.hex("visible_types", visible_);
    writer.number("live", memories_.live());
    writer.number("aliases", aliases_.live());
    uint64_t bytes = 0u;
    memories_.for_each([&bytes](const SourceMemory &memory) { bytes += memory.size(); });
    writer.number("bytes", bytes);
    writer.number("host_ram_held_bytes", host_ram_.held());
    uint64_t total = 0u;
    for (uint32_t heap = 0u; heap < host_.memoryHeapCount; heap++)
        if ((host_.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u) total += host_.memoryHeaps[heap].size;
    writer.number("vram_total_bytes", total);
    writer.number("vram_cap_bytes", vram_.cap_for(total));
    writer.number("vram_reserve_bytes", vram_.reserve_for(total));
    writer.close();
}

bool HostMemorySource::invariants_hold() const
{
    AV_SCOPE("HostMemorySource::invariants_hold");
    bool held = memories_.all_of([](const SourceMemory &memory) { return memory.references() != 0u; });
    memories_.for_each([this, &held](const SourceMemory &memory) {
        if (!memory.attached()) return;
        uint32_t answering = 0u;
        memories_.for_each([&memory, &answering](const SourceMemory &other) {
            answering += other.answers(memory.domain(), memory.key()) ? 1u : 0u;
        });
        held = held && answering == 1u;
    });
    return held && (visible_ & ~mappable_) == 0u && memories_.invariants_hold();
}

}
