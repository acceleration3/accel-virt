#ifndef AV_ICD_STAGED_MAPPING_HPP
#define AV_ICD_STAGED_MAPPING_HPP

#include <stdint.h>

namespace av::icd {

class StagedMapping {
public:
    enum class Memory : uint8_t { copy, named };

    StagedMapping();
    StagedMapping(uint64_t object, uint32_t subresource, uint64_t offset, uint64_t bytes, uint32_t flags);

    uint64_t object() const;
    uint32_t subresource() const;
    uint64_t offset() const;
    uint64_t bytes() const;
    uint32_t flags() const;
    void     reflag(uint32_t flags);
    uint8_t *at() const;
    bool     copied() const;
    bool     answers(const void *pointer) const;
    void     hand(uint8_t *at, Memory memory);

    bool invariants_hold() const;

private:
    uint64_t object_;
    uint64_t offset_;
    uint64_t bytes_;
    uint8_t *at_;
    uint32_t subresource_;
    uint32_t flags_;
    Memory   memory_;
};

}

#endif
