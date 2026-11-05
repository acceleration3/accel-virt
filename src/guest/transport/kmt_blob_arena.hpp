#ifndef AV_KMT_BLOB_ARENA_HPP
#define AV_KMT_BLOB_ARENA_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/transport/arena_segment.hpp"
#include "guest/transport/arena_slots.hpp"
#include "proto/client/client_blob_arena.hpp"

#include <stdint.h>

namespace av {

class DriverLog;
class GuestConnection;
class KmtTransport;
struct KmtBlobArenaProbe;

class KmtBlobArena {
public:
    static constexpr uint64_t default_bytes = uint64_t{16} << 20u;
    static constexpr uint64_t max_bytes = uint64_t{1} << 30u;
    static constexpr uint32_t max_regions = 8u;
    static constexpr uint64_t idle_release_ticks = uint64_t{100000000};

    KmtBlobArena(GuestConnection &connection, KmtTransport &transport, DriverLog &lines);
    ~KmtBlobArena();
    KmtBlobArena(const KmtBlobArena &) = delete;
    KmtBlobArena &operator=(const KmtBlobArena &) = delete;

    [[nodiscard]] Status open(uint64_t bytes);
    [[nodiscard]] Status close();
    [[nodiscard]] Status abandon();
    [[nodiscard]] Status grow(uint64_t need);
    [[nodiscard]] Status release_idle(uint64_t now);
    [[nodiscard]] Status open_region(uint64_t bytes, ArenaSegment *&region);
    [[nodiscard]] Status close_region(uint64_t base);
    [[nodiscard]] bool   take_query_slots(uint32_t count, uint8_t *&view, uint64_t &offset);
    bool                 lending() const;
    uint64_t             bytes() const;
    uint64_t             host_errors() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status open_control();
    [[nodiscard]] Status settle(bool abandoning);
    [[nodiscard]] Status add_ring(uint64_t want);
    bool                 first_refusal(Status refused);

    GuestConnection               &connection_;
    KmtTransport                  &transport_;
    DriverLog                     &lines_;
    ArenaSlots                     slots_;
    ObjectStorage<ArenaSegment>    control_;
    ObjectStorage<ArenaSegment>    rings_[ClientBlobArena::max_segments];
    ObjectStorage<ArenaSegment>    regions_[max_regions];
    bool                           slots_taken_;
    uint64_t                       idle_since_[ClientBlobArena::max_segments];
    uint32_t                       refusals_logged_;

    friend struct KmtBlobArenaProbe;
};

}

#endif
