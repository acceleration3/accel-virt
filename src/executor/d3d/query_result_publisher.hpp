/* Query results written where the guest reads them: the slots the guest named
 * with BindQueryResults, in its blob arena's published tail (PublishedSlots).
 * Every query ended is counted in the order the guest sent the ends, and a
 * result DXVK has is written to the query's slot with that count as its
 * sequence, so the guest's GetData reads memory instead of asking the host
 * and waiting for the answer to come back. A
 * pending query is referenced until its result is written or it is dropped;
 * any number may be pending, by the guest's id. */
#ifndef AV_QUERY_RESULT_PUBLISHER_HPP
#define AV_QUERY_RESULT_PUBLISHER_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "executor/published_slots.hpp"

#include <stdint.h>

struct ID3D11DeviceContext4;
struct ID3D11Query;

namespace av {

class BlockSource;
class GuestBytes;

class QueryResultPublisher {
public:
    /* The slots are in `bytes`, the context's arena. */
    QueryResultPublisher(BlockSource &blocks, GuestBytes &bytes);
    ~QueryResultPublisher();
    QueryResultPublisher(const QueryResultPublisher &) = delete;
    QueryResultPublisher &operator=(const QueryResultPublisher &) = delete;

    /* `slots` slots at `offset` of the region, as the guest bound them. False,
     * with none bound, for slots PublishedSlots refuses. */
    [[nodiscard]] bool bind(uint64_t offset, uint32_t slots);
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
    PublishedSlots slots_;
    uint64_t       ends_;
    bool     unflushed_;
    KeyedSlots<uint32_t, Pending> pending_;
};

}  // namespace av

#endif  // AV_QUERY_RESULT_PUBLISHER_HPP
