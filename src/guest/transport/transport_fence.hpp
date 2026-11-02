#ifndef AV_TRANSPORT_FENCE_HPP
#define AV_TRANSPORT_FENCE_HPP

#include <stdint.h>

namespace av {

struct TransportFenceProbe;

class TransportFence {
public:
    static constexpr uint32_t max_outstanding = 64u;

    TransportFence();

    uint64_t open();
    uint64_t peek() const;
    void     close(uint64_t fence_id);
    bool     pending(uint64_t fence_id) const;
    uint64_t completed() const;
    uint32_t outstanding() const;
    uint64_t oldest() const;
    bool     full() const;

    bool invariants_hold() const;

private:
    uint64_t next_;
    uint64_t completed_;

    friend struct TransportFenceProbe;
};

}

#endif
