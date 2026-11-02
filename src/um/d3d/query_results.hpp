#ifndef AV_UM_QUERY_RESULTS_HPP
#define AV_UM_QUERY_RESULTS_HPP

#include "guest/transport/published_slot_reader.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::d3d11 {

class QueryResults {
public:
    static constexpr uint32_t slots = 512u;

    QueryResults();
    QueryResults(const QueryResults &) = delete;
    QueryResults &operator=(const QueryResults &) = delete;

    void     take(uint8_t *view, uint64_t offset);
    void     bind(ClientTransport &wire);
    uint64_t ended();
    bool     read(uint32_t id, uint64_t end, void *data, uint32_t size) const;

private:
    PublishedSlotReader reader_;
    uint64_t            offset_;
    bool                bound_;
    uint64_t            ends_;
};

}

#endif
