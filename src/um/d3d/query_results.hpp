/* Where the host writes query results for this process to read: slots of its
 * blob arena's published tail (PublishedSlotLayout), named to the host with
 * BindQueryResults. Both sides count every EndQuery in the order the stream
 * carries them, and a slot holding a query's own count as its sequence holds
 * that end's result - so GetData reads memory rather than making a round trip
 * (QueryResultPublisher is the host's half, PublishedSlotReader what reads). */
#ifndef AV_UM_QUERY_RESULTS_HPP
#define AV_UM_QUERY_RESULTS_HPP

#include "guest/transport/published_slot_reader.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientBlobArena;
class ClientTransport;
}  // namespace av

namespace av::d3d11 {

class QueryResults {
public:
    static constexpr uint32_t slots = 512u;

    QueryResults();
    QueryResults(const QueryResults &) = delete;
    QueryResults &operator=(const QueryResults &) = delete;

    /* Takes the slots from the tail of the `bytes` at `view` the arena
     * lends, and lends it what lies before them. Nothing without an arena,
     * or one too small to lend as much again as the slots take. */
    void     reserve(ClientBlobArena &arena, uint8_t *view, uint64_t bytes);
    /* Names the slots to the host, once: it takes them only with a device made. */
    void     bind(ClientTransport &wire);
    /* The count of the end being sent now, which the host will write back. */
    uint64_t ended();
    /* The result of `id`'s end numbered `end`, into `data`: false while the
     * slot does not hold it whole, and always without bound slots. */
    bool     read(uint32_t id, uint64_t end, void *data, uint32_t size) const;

private:
    PublishedSlotReader reader_;
    uint64_t            offset_;
    bool                bound_;
    uint64_t            ends_;
};

}  // namespace av::d3d11

#endif  // AV_UM_QUERY_RESULTS_HPP
