/* One live map of an object an application writes or reads through a
 * pointer: the memory it was handed, and what that memory stands for - a
 * range of bytes of one subresource of one object, named as the driver names
 * it to the host.
 *
 * The memory is a private copy the map owns - filled at the map, sent back at
 * the unmap or at a flush point, and freed with the map - or memory the map
 * only names: a shadow the object keeps for its life, the arena the host's
 * object is made over, the application's own pages. Who fills the copy and
 * what goes back is each API's own (OpenGL's diff and flush marks, Direct3D
 * 11's write watch, OpenCL's whole range); this is only the bookkeeping they
 * share, kept by OpenMappings.
 *
 * Trivially copyable: OpenMappings keeps it, and the types API drivers derive
 * from it, by value. */
#ifndef AV_ICD_STAGED_MAPPING_HPP
#define AV_ICD_STAGED_MAPPING_HPP

#include <stdint.h>

namespace av::icd {

class StagedMapping {
public:
    /* Whose memory the map hands out. */
    enum class Memory : uint8_t { copy, named };

    StagedMapping();
    /* `bytes` of `subresource` of `object` from `offset`, mapped with the
     * API's own `flags`; no memory yet (OpenMappings hands it). */
    StagedMapping(uint64_t object, uint32_t subresource, uint64_t offset, uint64_t bytes, uint32_t flags);

    uint64_t object() const;
    uint32_t subresource() const;
    uint64_t offset() const;
    uint64_t bytes() const;
    uint32_t flags() const;
    /* A map of a subresource already mapped takes the newer map's flags. */
    void     reflag(uint32_t flags);
    uint8_t *at() const;
    /* Whether `at` is a private copy, freed with the map. */
    bool     copied() const;
    /* Whether `pointer` is the one this map answered. An unmap names the
     * pointer the map returned, not an address inside it. */
    bool     answers(const void *pointer) const;
    /* The memory the map hands out: OpenMappings' to give, once. */
    void     hand(uint8_t *at, Memory memory);

    /* A map names an object and memory, or is not yet handed any. */
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

}  // namespace av::icd

#endif  // AV_ICD_STAGED_MAPPING_HPP
