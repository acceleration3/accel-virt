/* Query results written where the guest reads them: the slots the guest named
 * with BindQueryResults, in its blob arena. Every query ended is counted in the
 * order the guest sent the ends, and a result DXVK has is written to the
 * query's slot with that count last, so the guest's GetData reads memory
 * instead of asking the host and waiting for the answer to come back. A
 * pending query is referenced until its result is written or it is dropped;
 * any number may be pending, by the guest's id. */
#ifndef AV_QUERY_RESULT_PUBLISHER_HPP
#define AV_QUERY_RESULT_PUBLISHER_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

struct ID3D11DeviceContext4;
struct ID3D11Query;

namespace av {

class BlockSource;

class QueryResultPublisher {
public:
    static constexpr uint32_t slot_bytes = 128u;
    static constexpr uint32_t data_at = 16u;
    explicit QueryResultPublisher(BlockSource &blocks);
    ~QueryResultPublisher();
    QueryResultPublisher(const QueryResultPublisher &) = delete;
    QueryResultPublisher &operator=(const QueryResultPublisher &) = delete;

    /* `slots` slots at `results`, which stays the guest's memory until unbind. */
    void bind(uint8_t *results, uint32_t slots);
    void unbind();
    /* Every pending query's reference given back, the list emptied. */
    void release_all();
    /* Every end is counted, published or not, as the guest counts them; a
     * query ended again before its result was written supersedes its last
     * end. no_memory, the query not pending, when it cannot be kept. */
    [[nodiscard]] Status ended(uint32_t id, ID3D11Query *query);
    /* The query is destroyed: nothing of it is published any more. */
    void forget(uint32_t id);
    /* An end since the last flush, whose result DXVK has not submitted. */
    bool needs_flush() const;
    void flushed();
    /* Writes whatever DXVK has; true while anything is still pending. */
    bool publish(ID3D11DeviceContext4 *context);
    bool pending() const;

private:
    struct Pending {
        ID3D11Query *query_;
        uint64_t     end_;
    };
    void write(uint32_t id, const uint8_t *data, uint32_t size, uint64_t end) const;

    uint8_t *results_;
    uint32_t slots_;
    uint64_t ends_;
    bool     unflushed_;
    KeyedSlots<uint32_t, Pending> pending_;
};

}  // namespace av

#endif  // AV_QUERY_RESULT_PUBLISHER_HPP
