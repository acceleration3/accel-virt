#include "executor/source_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "proto/wire/guest_bytes.hpp"


namespace av {

SourceMemory::SourceMemory(uint32_t domain, uint64_t key, uint32_t memory_type, uint32_t caching,
                           const ExportedMemory &memory)
    : domain_(domain), key_(key), memory_type_(memory_type), caching_(caching), memory_(memory), attached_(true),
      references_(1u)
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

uint8_t *SourceMemory::pointer() const
{
    AV_SCOPE("SourceMemory::pointer");
    return memory_.pointer();
}

uint8_t *SourceMemory::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("SourceMemory::at");
    return GuestBytes(memory_.pointer(), memory_.size()).at(offset, bytes);
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

void SourceMemory::detach()
{
    AV_SCOPE("SourceMemory::detach");
    attached_ = false;
}

void SourceMemory::acquire()
{
    AV_SCOPE("SourceMemory::acquire");
    references_++;
}

bool SourceMemory::release()
{
    AV_SCOPE("SourceMemory::release");
    references_--;
    return references_ == 0u;
}

bool SourceMemory::attached() const
{
    AV_SCOPE("SourceMemory::attached");
    return attached_;
}

uint32_t SourceMemory::references() const
{
    AV_SCOPE("SourceMemory::references");
    return references_;
}

}  // namespace av
