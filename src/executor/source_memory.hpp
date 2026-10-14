/* One allocation of a host GPU's memory source (HostMemorySource): memory a
 * guest asked for under one key of one domain, which the device maps into the
 * host window and which the guest's contexts import. Counted: the device's
 * blob holds one reference and every import one more, so memory a guest
 * releases while a host API still has it lives until that import goes. */
#ifndef AV_SOURCE_MEMORY_HPP
#define AV_SOURCE_MEMORY_HPP

#include "executor/exported_memory.hpp"

#include <stdint.h>

namespace av {

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
    const ExportedMemory &exported() const;
    /* A new descriptor of its dma-buf, the caller's: what a host API imports
     * it by. -1 when none can be had. */
    int                   share() const;

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
};

}  // namespace av

#endif  // AV_SOURCE_MEMORY_HPP
