/* One mapped range of a buffer object (data-model.md, research.md §5). Neither
 * host driver can map guest memory, so the pointer an application writes
 * through is a copy the guest holds: filled from the host at map unless the
 * map invalidates or only writes, and written back when the application is
 * done - at unmap, or before the next command that reads buffers for a
 * persistent map. With explicit flushing only the ranges the application
 * flushed go back. */
#ifndef AV_GLICD_BUFFER_MAPPING_HPP
#define AV_GLICD_BUFFER_MAPPING_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::glicd {

struct BufferMappingProbe;

class BufferMapping {
public:
    enum : uint32_t {
        map_read = 0x0001u, map_write = 0x0002u, map_invalidate_range = 0x0004u, map_invalidate_buffer = 0x0008u,
        map_flush_explicit = 0x0010u, map_unsynchronized = 0x0020u, map_persistent = 0x0040u,
    };
    /* What one read asks for: the reply slot holds it with room for its length. */
    static constexpr size_t   read_bytes = size_t{48} << 10u;
    /* What one write sends: a quarter of the smallest arena through a lease,
     * and what fits a command's staging inline. */
    static constexpr size_t   blob_write_bytes = size_t{4} << 20u;
    static constexpr size_t   inline_write_bytes = size_t{192} << 10u;
    static constexpr uint64_t max_length = uint64_t{1} << 30u;

    explicit BufferMapping(BlockSource &memory);
    ~BufferMapping();
    BufferMapping(const BufferMapping &) = delete;
    BufferMapping &operator=(const BufferMapping &) = delete;

    /* `length` bytes of `buffer` from `offset` as the application's memory.
     * invalid_state when already mapped; invalid_parameter for no buffer, no
     * bytes or more than max_length; no_memory. */
    [[nodiscard]] Status map(ClientTransport &wire, uint32_t buffer, uint64_t offset, uint64_t length,
                             uint32_t access);
    uint32_t buffer() const;
    void    *pointer() const;
    uint64_t offset() const;
    uint64_t length() const;
    uint32_t access() const;
    /* A persistent map the application writes: written back before every
     * command that reads buffers. */
    bool     persistent_writes() const;
    /* glFlushMappedBufferRange, relative to the mapped range: invalid_state
     * unless the map writes with explicit flushing; invalid_parameter for a
     * range past the mapping. */
    [[nodiscard]] Status mark(uint64_t offset, uint64_t length);
    /* What the application wrote, to the host: the whole range, or with
     * explicit flushing what was marked since the last write-back. Nothing
     * for a map that does not write. */
    void     write_back(ClientTransport &wire);

    void report(StateWriter &writer, uint32_t index) const;

    bool invariants_hold() const;

private:
    void   read(ClientTransport &wire);
    /* The next write's bytes: a lease's worth when the arena has room for it
     * - after asking the host something, which retires the arena, if it has
     * not - and an inline command's otherwise. */
    size_t chunk_for(ClientTransport &wire, uint64_t left) const;

    BlockSource &memory_;
    uint8_t     *block_;
    uint32_t     buffer_;
    uint64_t     offset_;
    uint64_t     length_;
    uint32_t     access_;
    uint64_t     dirty_from_;
    uint64_t     dirty_to_;

    friend struct BufferMappingProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_BUFFER_MAPPING_HPP
