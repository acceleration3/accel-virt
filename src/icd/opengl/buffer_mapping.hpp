/* One mapped range of a buffer object (data-model.md, research.md §5), kept
 * by MappedBuffers' OpenMappings. Neither host driver can map guest memory,
 * so the pointer an application writes through is a private copy, filled at
 * the map and written back when the application is done - at unmap, or
 * before the next command that reads buffers for a persistent map
 * (BufferMapTransfer). With explicit flushing only the ranges the
 * application flushed go back: this keeps what it marked. */
#ifndef AV_GLICD_BUFFER_MAPPING_HPP
#define AV_GLICD_BUFFER_MAPPING_HPP

#include "core/support/status.hpp"
#include "icd/common/staged_mapping.hpp"

#include <stdint.h>

namespace av {
class StateWriter;
}  // namespace av

namespace av::glicd {

class BufferMapping : public icd::StagedMapping {
public:
    enum : uint32_t {
        map_read = 0x0001u, map_write = 0x0002u, map_invalidate_range = 0x0004u, map_invalidate_buffer = 0x0008u,
        map_flush_explicit = 0x0010u, map_unsynchronized = 0x0020u, map_persistent = 0x0040u,
    };
    static constexpr uint64_t max_length = uint64_t{1} << 30u;

    BufferMapping();
    /* `length` bytes of `buffer` from `offset`, mapped with `access`. */
    BufferMapping(uint32_t buffer, uint64_t offset, uint64_t length, uint32_t access);

    uint32_t buffer() const;
    uint32_t access() const;
    /* A persistent map the application writes: written back before every
     * command that reads buffers. */
    bool     persistent_writes() const;
    /* glFlushMappedBufferRange, relative to the mapped range: invalid_state
     * unless the map writes with explicit flushing; invalid_parameter for a
     * range past the mapping. */
    [[nodiscard]] Status mark(uint64_t offset, uint64_t length);
    /* What was marked since the last take, as [from, to) of the range;
     * nothing is marked after. */
    void     take_marked(uint64_t &from, uint64_t &to);

    void report(StateWriter &writer, uint32_t index) const;

    /* What is marked lies inside the range, only on an explicitly flushed
     * write map. */
    bool invariants_hold() const;

private:
    uint64_t dirty_from_;
    uint64_t dirty_to_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_BUFFER_MAPPING_HPP
