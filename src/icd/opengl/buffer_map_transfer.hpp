/* What a buffer map's private copy starts from, and what of it goes back to
 * the host - OpenGL's policy over the copies OpenMappings keeps.
 *
 * OpenGL keeps every byte a map does not write, so a map that writes starts
 * from what the host holds: the guest's copy of it (BufferShadows) when there
 * is one, the host's bytes otherwise - except where nothing needs keeping,
 * because the map invalidates, flushes only what it marks, or the storage is
 * fresh. A write-back against a copy sends only the words that differ from
 * it, as WrittenRuns merges them; one without sends the whole range, which
 * then becomes the copy; one with explicit flushing sends what was marked. */
#ifndef AV_GLICD_BUFFER_MAP_TRANSFER_HPP
#define AV_GLICD_BUFFER_MAP_TRANSFER_HPP

#include "icd/common/written_runs.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::glicd {

class BufferMapping;
class BufferShadows;

class BufferMapTransfer {
public:
    explicit BufferMapTransfer(BufferShadows &shadows);
    BufferMapTransfer(const BufferMapTransfer &) = delete;
    BufferMapTransfer &operator=(const BufferMapTransfer &) = delete;

    /* The application's first view of a map's copy, which starts zeroed. */
    void start(ClientTransport &wire, BufferMapping &mapping);
    /* What the application wrote through the map, to the host. Nothing for
     * a map that does not write. */
    void write_back(ClientTransport &wire, BufferMapping &mapping);

private:
    static constexpr uint64_t word = 8u;

    void     read(ClientTransport &wire, const BufferMapping &mapping);
    /* Bytes [at, end) of the map's range. */
    void     send(ClientTransport &wire, const BufferMapping &mapping, uint64_t at, uint64_t end);
    /* The runs of the copy that differ from `known`, which then holds them
     * too. */
    void     send_changes(ClientTransport &wire, const BufferMapping &mapping, uint8_t *known);
    void     send_runs(ClientTransport &wire, const BufferMapping &mapping, uint8_t *known);
    /* Words step from the start of the range, the last one short when the
     * length is not a multiple of one. The first word from `at` that differs
     * from `known`; the length if none. */
    uint64_t next_change(const BufferMapping &mapping, const uint8_t *known, uint64_t at) const;
    /* Where the words that differ from `from` on, one after another, end. */
    uint64_t change_end(const BufferMapping &mapping, const uint8_t *known, uint64_t from) const;

    BufferShadows   &shadows_;
    icd::WrittenRuns runs_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_BUFFER_MAP_TRANSFER_HPP
