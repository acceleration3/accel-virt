#include "executor/source_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"


namespace av {

SourceMemory::SourceMemory(uint32_t domain, uint64_t key, uint32_t memory_type, uint32_t caching,
                           const ExportedMemory &memory)
    : domain_(domain), key_(key), memory_type_(memory_type), caching_(caching), memory_(memory), attached_(true),
      references_(1u), owner_(nullptr), charged_(0u), charged_device_(false)
{
    AV_SCOPE("SourceMemory::SourceMemory");
}

bool SourceMemory::answers(uint32_t domain, uint64_t key) const
{
    AV_SCOPE("SourceMemory::answers");
    return attached_ && domain_ == domain && key_ == key;
}

uint32_t SourceMemory::domain() const
{
    AV_SCOPE("SourceMemory::domain");
    return domain_;
}

uint64_t SourceMemory::key() const
{
    AV_SCOPE("SourceMemory::key");
    return key_;
}

uint32_t SourceMemory::memory_type() const
{
    AV_SCOPE("SourceMemory::memory_type");
    return memory_type_;
}

uint32_t SourceMemory::caching() const
{
    AV_SCOPE("SourceMemory::caching");
    return caching_;
}

uint64_t SourceMemory::size() const
{
    AV_SCOPE("SourceMemory::size");
    return memory_.size();
}

uint64_t SourceMemory::charged() const
{
    AV_SCOPE("SourceMemory::charged");
    return charged_;
}

void SourceMemory::charge(uint64_t bytes, bool device)
{
    AV_SCOPE("SourceMemory::charge");
    charged_ = bytes;
    charged_device_ = device;
}

bool SourceMemory::charged_device() const
{
    AV_SCOPE("SourceMemory::charged_device");
    return charged_device_;
}

uint8_t *SourceMemory::pointer() const
{
    AV_SCOPE("SourceMemory::pointer");
    return memory_.pointer();
}

uint8_t *SourceMemory::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("SourceMemory::at");
    const uint64_t size = memory_.size();
    if (memory_.pointer() == nullptr || size == 0u || offset > size || bytes > size - offset) return nullptr;
    return memory_.pointer() + static_cast<uintptr_t>(offset);
}

const ExportedMemory &SourceMemory::exported() const
{
    AV_SCOPE("SourceMemory::exported");
    return memory_;
}

bool SourceMemory::share(OwnedFd &into) const
{
    AV_SCOPE("SourceMemory::share");
    return into.hold_copy(memory_.fd());
}

void SourceMemory::bind(HostMemorySource &owner)
{
    AV_SCOPE("SourceMemory::bind");
    owner_ = &owner;
}

HostMemorySource *SourceMemory::owner() const
{
    AV_SCOPE("SourceMemory::owner");
    return owner_;
}

void SourceMemory::detach()
{
    AV_SCOPE("SourceMemory::detach");
    attached_ = false;
}

void SourceMemory::acquire()
{
    AV_SCOPE("SourceMemory::acquire");
    __atomic_add_fetch(&references_, 1u, __ATOMIC_ACQ_REL);
}

bool SourceMemory::release()
{
    AV_SCOPE("SourceMemory::release");
    return __atomic_sub_fetch(&references_, 1u, __ATOMIC_ACQ_REL) == 0u;
}

bool SourceMemory::attached() const
{
    AV_SCOPE("SourceMemory::attached");
    return attached_;
}

uint32_t SourceMemory::references() const
{
    AV_SCOPE("SourceMemory::references");
    return __atomic_load_n(&references_, __ATOMIC_ACQUIRE);
}

}
