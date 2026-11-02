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
    QueryResultPublisher(BlockSource &blocks, GuestBytes &bytes);
    ~QueryResultPublisher();
    QueryResultPublisher(const QueryResultPublisher &) = delete;
    QueryResultPublisher &operator=(const QueryResultPublisher &) = delete;

    [[nodiscard]] bool bind(uint64_t offset, uint32_t slots);
    void unbind();
    void release_all();
    [[nodiscard]] Status ended(uint32_t id, ID3D11Query *query);
    void forget(uint32_t id);
    bool needs_flush() const;
    void flushed();
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

}

#endif
