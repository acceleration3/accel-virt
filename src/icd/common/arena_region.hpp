#ifndef AV_ICD_ARENA_REGION_HPP
#define AV_ICD_ARENA_REGION_HPP

#include "proto/client/client_blob_lease.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::icd {

class ArenaRegion {
public:
    ArenaRegion();
    ~ArenaRegion();
    ArenaRegion(const ArenaRegion &) = delete;
    ArenaRegion &operator=(const ArenaRegion &) = delete;

    [[nodiscard]] bool open(ClientTransport &wire, uint64_t bytes);
    void               close();

    bool     held() const;
    uint8_t *view() const;
    uint64_t offset() const;
    uint64_t bytes() const;

    bool invariants_hold() const;

private:
    ClientTransport *wire_;
    ClientBlobLease  lease_;
};

}

#endif
