#include "executor/host_memory_source.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "executor/memory_exporter.hpp"

namespace av {

HostMemorySource::HostMemorySource(MemoryExporter &exporter, BlockSource &blocks)
    : exporter_(exporter), memories_(blocks), mutex_(PTHREAD_MUTEX_INITIALIZER), asked_(0u), mappable_(0u)
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
        if (shared && exporter_.exportable(type)) mappable_ |= bit;
    }
    return mappable_;
}

uint32_t HostMemorySource::first_type(uint32_t wanted, uint32_t refused)
{
    AV_SCOPE("HostMemorySource::first_type");
    for (uint32_t type = 0u; type < HostMemoryRequest::memory_types; type++) {
        if ((mappable_ & (1u << type)) == 0u) continue;
        const uint32_t flags = exporter_.memory_flags(type);
        if ((flags & wanted) == wanted && (flags & refused) == 0u) return type;
    }
    return HostMemoryRequest::memory_types;
}

/* Upload memory is what the CPU writes and the GPU reads: the GPU's own,
 * through a resizable BAR, where there is one. Readback memory is what a CPU
 * reads, the guest's or the host's, which only cached memory serves at
 * speed. A Vulkan request names its type. */
uint32_t HostMemorySource::choose(HostMemoryRequest request)
{
    AV_SCOPE("HostMemorySource::choose");
    uint32_t type = HostMemoryRequest::memory_types;
    switch (request.kind()) {
    case HostMemoryRequest::Kind::vulkan:
        return (mappable_ & (1u << request.vulkan_type())) != 0u ? request.vulkan_type()
                                                                 : HostMemoryRequest::memory_types;
    case HostMemoryRequest::Kind::upload:
        type = first_type(device_local, host_cached);
        if (type == HostMemoryRequest::memory_types) type = first_type(0u, host_cached);
        break;
    case HostMemoryRequest::Kind::readback: type = first_type(host_cached, 0u); break;
    }
    return type != HostMemoryRequest::memory_types ? type : first_type(0u, 0u);
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
    const uint32_t type = choose(request);
    if (type == HostMemoryRequest::memory_types) return Status::invalid_parameter;
    ExportedMemory exported;
    if (exporter_.allocate(type, size, exported) != Status::ok) return Status::no_memory;
    const int shared = exporter_.share(exported);
    SourceMemory *made = nullptr;
    const uint32_t caching = (exporter_.memory_flags(type) & host_cached) != 0u ? AV_MEMORY_WRITE_BACK
                                                                               : AV_MEMORY_WRITE_COMBINED;
    if (shared < 0 || memories_.create(made, domain, key, type, caching, exported) != Status::ok) {
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
    return held && memories_.invariants_hold();
}

}  // namespace av
