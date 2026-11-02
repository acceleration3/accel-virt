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

    bool                  answers(uint32_t domain, uint64_t key) const;
    uint32_t              domain() const;
    uint64_t              key() const;
    uint32_t              memory_type() const;
    uint32_t              caching() const;
    uint64_t              size() const;
    uint8_t              *pointer() const;
    uint8_t              *at(uint64_t offset, uint64_t bytes) const;
    const ExportedMemory &exported() const;
    [[nodiscard]] bool    share(OwnedFd &into) const;

    void                  bind(HostMemorySource &owner);
    HostMemorySource     *owner() const;

    void detach();
    void acquire();
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

}

#endif
