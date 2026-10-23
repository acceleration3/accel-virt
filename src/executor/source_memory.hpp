/* One allocation of a host GPU's memory source (HostMemorySource): memory a
 * guest asked for under one key of one domain, which the device maps into the
 * host window and which the guest's contexts import. Counted: the device's
 * blob holds one reference and every import one more, so memory a guest
 * releases while a host API still has it lives until that import goes. The
 * count is atomic: another GPU's source names the memory under a key of its
 * own (HostMemorySource::alias) and takes references to it under its own lock,
 * with no lock of the allocating source's - two sources aliasing each other's
 * memory would otherwise take their locks in opposite orders. The last
 * release is the allocating source's to act on, under its lock. */
#ifndef AV_SOURCE_MEMORY_HPP
#define AV_SOURCE_MEMORY_HPP

#include "executor/exported_memory.hpp"

#include <stdint.h>

namespace av {

class HostMemorySource;
class OwnedFd;

class SourceMemory {
public:
    SourceMemory(uint32_t domain, uint64_t key, uint32_t memory_type, uint32_t caching, const ExportedMemory &memory);
    SourceMemory(const SourceMemory &) = delete;
    SourceMemory &operator=(const SourceMemory &) = delete;

    /* Whether this is the live allocation under `key` in `domain`: one whose
     * key was freed no longer answers to it, so the guest can reuse the key. */
    bool                  answers(uint32_t domain, uint64_t key) const;
    uint32_t              domain() const;
    uint64_t              key() const;
    /* The host memory type it was allocated as. */
    uint32_t              memory_type() const;
    /* AV_MEMORY_*: how it is mapped, here and in the guest. */
    uint32_t              caching() const;
    uint64_t              size() const;
    /* The source's own mapping of it, for an API that copies through it. */
    uint8_t              *pointer() const;
    /* `bytes` at `offset` of that mapping, or null: for memory with no
     * mapping, an offset or a length past its end, and a sum that would wrap -
     * every number a command names it by came off the wire. */
    uint8_t              *at(uint64_t offset, uint64_t bytes) const;
    const ExportedMemory &exported() const;
    /* A new descriptor of its dma-buf held in `into`: what a host API imports
     * it by. False, holding nothing, when none can be had. */
    [[nodiscard]] bool    share(OwnedFd &into) const;

    /* The source that allocated it, which counts its references: another
     * GPU's source may name it under a key of its own (HostMemorySource::alias)
     * and an import through that key gives it back here. Null until bound. */
    void                  bind(HostMemorySource &owner);
    HostMemorySource     *owner() const;

    /* The key let go: the device released its blob. */
    void detach();
    void acquire();
    /* One reference let go: true when it was the last. */
    bool release();
    bool attached() const;
    uint32_t references() const;

private:
    uint32_t       domain_;
    uint64_t       key_;
    uint32_t       memory_type_;
    uint32_t       caching_;
    ExportedMemory memory_;
    bool           attached_;
    uint32_t       references_;
    HostMemorySource *owner_;
};

}  // namespace av

#endif  // AV_SOURCE_MEMORY_HPP
