#ifndef AV_HOST_WATCH_HPP
#define AV_HOST_WATCH_HPP

#include <stdint.h>

namespace av {

class Kmt;
class TransportRing;
struct HostWatchProbe;

class HostWatch {
public:
    static constexpr uint64_t silence_seconds = 30u;

    HostWatch(Kmt &kmt, const TransportRing &ring);
    HostWatch(const HostWatch &) = delete;
    HostWatch &operator=(const HostWatch &) = delete;

    void     read_page(const uint8_t *page);
    bool     reads_page() const;
    uint64_t completed() const;
    bool     faulted() const;
    uint32_t fault_code() const;

    void     start();
    bool     keeps_waiting();
    bool invariants_hold() const;

private:
    bool     moved();
    uint64_t word(uint32_t offset) const;

    Kmt                 &kmt_;
    const TransportRing &ring_;
    const uint8_t       *page_;
    uint64_t             head_;
    uint64_t             completed_;
    uint64_t             progress_;
    uint64_t             beat_;
    uint64_t             last_moved_;
    uint64_t             now_;
    bool                 timing_;

    friend struct HostWatchProbe;
};

}

#endif
