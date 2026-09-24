/* Where the host writes query results for this process to read: the last
 * slots of its blob arena, named to the host with BindQueryResults. Both sides
 * count every EndQuery in the order the stream carries them, and a slot holding
 * a query's own count holds that end's result - so GetData reads memory rather
 * than making a round trip (QueryResultPublisher is the host's half). */
#ifndef AV_UM_QUERY_RESULTS_HPP
#define AV_UM_QUERY_RESULTS_HPP

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
    static constexpr uint32_t slot_bytes = 128u;
    static constexpr uint32_t data_at = 16u;

    QueryResults();
    QueryResults(const QueryResults &) = delete;
    QueryResults &operator=(const QueryResults &) = delete;

    /* Takes the slots off the end of the `bytes` at `view` the arena lends,
     * and lends it the rest. Nothing without an arena. */
    void     reserve(ClientBlobArena &arena, uint8_t *view, uint64_t bytes);
    /* Names the slots to the host, once: it takes them only with a device made. */
    void     bind(ClientTransport &wire);
    /* The count of the end being sent now, which the host will write back. */
    uint64_t ended();
    /* The result of `id`'s end numbered `end`, into `data`: false while the
     * slot does not hold it, and always without bound slots. */
    bool     read(uint32_t id, uint64_t end, void *data, uint32_t size) const;

private:
    uint8_t *region_;
    uint64_t offset_;
    bool     bound_;
    uint64_t ends_;
};

}  // namespace av::d3d11

#endif  // AV_UM_QUERY_RESULTS_HPP
