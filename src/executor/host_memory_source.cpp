#include "executor/host_memory_source.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "executor/memory_exporter.hpp"

#include <unistd.h>

namespace av {

HostMemorySource::HostMemorySource(MemoryExporter &exporter, BlockSource &blocks)
    : exporter_(exporter), memories_(blocks), mutex_(PTHREAD_MUTEX_INITIALIZER), asked_(0u), mappable_(0u), visible_(0u)
{
    AV_SCOPE("HostMemorySource::HostMemorySource");
}

HostMemorySource::~HostMemorySource()
{
    AV_SCOPE("HostMemorySource::~HostMemorySource");
    memories_.for_each([this](const SourceMemory &memory) { exporter_.free(memory.exported()); });
    memories_.destroy_all();
    pthread_mutex_destroy(&mutex_);
}

/* A type is asked of the exporter once, the first time anything wants to
 * know: the probe allocates on the GPU, and nothing allocates before a guest
 * asks. */
uint32_t HostMemorySource::mappable_types()
{
    AV_SCOPE("HostMemorySource::mappable_types");
    pthread_mutex_lock(&mutex_);
    ScopeExit      unlock([this] { pthread_mutex_unlock(&mutex_); });
    const uint32_t count = exporter_.memory_types();
    for (uint32_t type = 0u; type < count && type < HostMemoryRequest::memory_types; type++) {
        const uint32_t bit = 1u << type;
        if ((asked_ & bit) != 0u) continue;
        asked_ |= bit;
        const uint32_t flags = exporter_.memory_flags(type);
        const bool     shared = (flags & (host_visible | host_coherent)) == (host_visible | host_coherent);
        if (!shared || !exporter_.exportable(type)) continue;
        mappable_ |= bit;
        if (exporter_.imports_host_visible(type)) visible_ |= bit;
    }
    return mappable_;
}

uint32_t HostMemorySource::first_type(uint32_t among, uint32_t wanted, uint32_t refused)
{
    AV_SCOPE("HostMemorySource::first_type");
    for (uint32_t type = 0u; type < HostMemoryRequest::memory_types; type++) {
        if ((among & (1u << type)) == 0u) continue;
        const uint32_t flags = exporter_.memory_flags(type);
        if ((flags & wanted) == wanted && (flags & refused) == 0u) return type;
    }
    return HostMemoryRequest::memory_types;
}

/* Upload memory is what the CPU writes and the GPU reads: the GPU's own,
 * through a resizable BAR, where there is one, else uncached system memory.
 * Readback memory is what a CPU reads, the guest's or the host's, which only
 * cached memory serves at speed. Either is first sought among the types an
 * importer may map, since vkd3d-proton imports a heap only as a type the
 * driver reports for its dma-buf: NVIDIA's BAR memory would leave every upload
 * heap refused. A Vulkan request names its type, and has no other: the
 * backend imports it as that type. */
uint32_t HostMemorySource::choices(HostMemoryRequest request, uint32_t (&types)[most_choices])
{
    AV_SCOPE("HostMemorySource::choices");
    uint32_t found[most_choices] = {};
    for (uint32_t &type : found) type = HostMemoryRequest::memory_types;
    constexpr uint32_t per_group = most_choices / 2u;
    const uint32_t     groups[2] = {visible_, mappable_ & ~visible_};
    /* The request was judged valid, so a kind that is neither of the first
     * two is readback. */
    if (request.kind() == HostMemoryRequest::Kind::vulkan) {
        if ((mappable_ & (1u << request.vulkan_type())) != 0u) found[0] = request.vulkan_type();
    } else if (request.kind() == HostMemoryRequest::Kind::upload) {
        for (uint32_t group = 0u; group < 2u; group++) {
            found[group * per_group] = first_type(groups[group], device_local, host_cached);
            found[group * per_group + 1u] = first_type(groups[group], 0u, device_local | host_cached);
            found[group * per_group + 2u] = first_type(groups[group], 0u, 0u);
        }
    } else {
        for (uint32_t group = 0u; group < 2u; group++) {
            found[group * per_group] = first_type(groups[group], host_cached, 0u);
            found[group * per_group + 1u] = first_type(groups[group], 0u, 0u);
        }
    }
    uint32_t count = 0u;
    for (const uint32_t type : found) {
        bool seen = type == HostMemoryRequest::memory_types;
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
    static_cast<void>(mappable_types());
    pthread_mutex_lock(&mutex_);
    ScopeExit unlock([this] { pthread_mutex_unlock(&mutex_); });
    if (find(domain, key) != nullptr) return Status::conflict;
    uint32_t       types[most_choices] = {};
    const uint32_t count = choices(request, types);
    if (count == 0u) return Status::invalid_parameter;
    ExportedMemory exported;
    uint32_t       type = HostMemoryRequest::memory_types;
    for (uint32_t choice = 0u; choice < count && type == HostMemoryRequest::memory_types; choice++)
        if (exporter_.allocate(types[choice], size, exported) == Status::ok) type = types[choice];
    if (type == HostMemoryRequest::memory_types) return Status::no_memory;
    const int shared = exporter_.share(exported);
    SourceMemory *made = nullptr;
    const uint32_t caching = (exporter_.memory_flags(type) & host_cached) != 0u ? AV_MEMORY_WRITE_BACK
                                                                               : AV_MEMORY_WRITE_COMBINED;
    if (shared < 0 || memories_.create(made, domain, key, type, caching, exported) != Status::ok) {
        /* The descriptor shared for the caller is nobody's if the memory is. */
        if (shared >= 0) close(shared);
        exporter_.free(exported);
        return Status::no_memory;
    }
    memory.fd = shared;
    memory.caching = caching;
    memory.size = exported.size();
    return Status::ok;
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
    static_cast<void>(memories_.destroy(&memory));
}

void HostMemorySource::free(uint32_t domain, uint64_t key)
{
    AV_SCOPE("HostMemorySource::free");
    pthread_mutex_lock(&mutex_);
    ScopeExit           unlock([this] { pthread_mutex_unlock(&mutex_); });
    SourceMemory *const memory = find(domain, key);
    if (memory == nullptr) return;
    memory->detach();
    let_go(*memory);
}

void HostMemorySource::free_domain(uint32_t domain)
{
    AV_SCOPE("HostMemorySource::free_domain");
    pthread_mutex_lock(&mutex_);
    ScopeExit unlock([this] { pthread_mutex_unlock(&mutex_); });
    for (SourceMemory *memory = nullptr;
         (memory = memories_.find_if([domain](const SourceMemory &held) {
              return held.attached() && held.domain() == domain;
          })) != nullptr;) {
        memory->detach();
        let_go(*memory);
    }
}

SourceMemory *HostMemorySource::acquire(uint32_t domain, uint64_t key)
{
    AV_SCOPE("HostMemorySource::acquire");
    pthread_mutex_lock(&mutex_);
    ScopeExit           unlock([this] { pthread_mutex_unlock(&mutex_); });
    SourceMemory *const memory = find(domain, key);
    if (memory != nullptr) memory->acquire();
    return memory;
}

void HostMemorySource::release(SourceMemory &memory)
{
    AV_SCOPE("HostMemorySource::release");
    pthread_mutex_lock(&mutex_);
    let_go(memory);
    pthread_mutex_unlock(&mutex_);
}

uint32_t HostMemorySource::live() const
{
    AV_SCOPE("HostMemorySource::live");
    return memories_.live();
}

void HostMemorySource::report(StateWriter &writer) const
{
    AV_SCOPE("HostMemorySource::report");
    writer.open("memory_source", 0u);
    writer.hex("mappable_types", mappable_);
    writer.hex("visible_types", visible_);
    writer.number("live", memories_.live());
    uint64_t bytes = 0u;
    memories_.for_each([&bytes](const SourceMemory &memory) { bytes += memory.size(); });
    writer.number("bytes", bytes);
    writer.close();
}

/* Every allocation is still referenced by someone, and no two live ones
 * answer to one key. */
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

}  // namespace av
