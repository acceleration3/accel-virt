/* One mapped range of a buffer object (data-model.md, research.md §5). Neither
 * host driver can map guest memory, so the pointer an application writes
 * through is a copy the guest holds, and written back when the application is
 * done - at unmap, or before the next command that reads buffers for a
 * persistent map. With explicit flushing only the ranges the application
 * flushed go back.
 *
 * OpenGL keeps every byte a map does not write, so a map that writes starts
 * from what the host holds: the guest's copy of it (BufferShadows) when there
 * is one, the host's bytes otherwise - except where nothing needs keeping,
 * because the map invalidates, flushes only what it marks, or the storage is
 * fresh. A write-back against a copy sends only the words that differ from
 * it; one without sends the whole range, which then becomes the copy. */
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

class BufferShadows;
struct BufferMappingProbe;

class BufferMapping {
public:
    enum : uint32_t {
        map_read = 0x0001u, map_write = 0x0002u, map_invalidate_range = 0x0004u, map_invalidate_buffer = 0x0008u,
        map_flush_explicit = 0x0010u, map_unsynchronized = 0x0020u, map_persistent = 0x0040u,
    };
    static constexpr uint64_t max_length = uint64_t{1} << 30u;
    /* Changes closer than this go back as one write: the bytes between are
     * the host's own, and a write costs a command. */
    static constexpr uint64_t merge_gap = 256u;

    BufferMapping(BlockSource &memory, BufferShadows &shadows);
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
    /* What the application wrote, to the host: the words that differ from
     * the guest's copy, the whole range without one, or with explicit
     * flushing what was marked since the last write-back. Nothing for a map
     * that does not write. */
    void     write_back(ClientTransport &wire);

    void report(StateWriter &writer, uint32_t index) const;

    bool invariants_hold() const;

private:
    static constexpr uint64_t word = 8u;

    /* The application's first view of the range (see above). */
    void     start(ClientTransport &wire);
    void     read(ClientTransport &wire);
    /* Bytes [at, end) of the range. */
    void     send(ClientTransport &wire, uint64_t at, uint64_t end);
    /* The runs that differ from `known`, which then holds them too. */
    void     send_changes(ClientTransport &wire, uint8_t *known);
    /* Words step from the start of the range, the last one short when the
     * length is not a multiple of one. The first word from `at` that
     * differs from `known`; length_ if none. */
    uint64_t next_change(const uint8_t *known, uint64_t at) const;
    /* Where the run of changes from `from` ends: past its last changed word
     * before merge_gap bytes in a row stay the same. */
    uint64_t change_end(const uint8_t *known, uint64_t from) const;

    BlockSource   &memory_;
    BufferShadows &shadows_;
    uint8_t       *block_;
    uint32_t       buffer_;
    uint64_t       offset_;
    uint64_t       length_;
    uint32_t       access_;
    uint64_t       dirty_from_;
    uint64_t       dirty_to_;

    friend struct BufferMappingProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_BUFFER_MAPPING_HPP
