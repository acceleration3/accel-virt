#ifndef AV_VBLANK_SCHEDULE_HPP
#define AV_VBLANK_SCHEDULE_HPP

#include <stdint.h>

namespace av {


class VblankSchedule {
public:
    static constexpr uint32_t default_millihz = 60000u;
    static constexpr uint64_t most_overdue = 1u << 20u;

    VblankSchedule();

    void     start(uint64_t now);
    bool     running() const;
    void     retime(uint32_t millihz, uint64_t now);
    void     align(uint32_t microhz, int64_t shift, uint64_t now);
    uint32_t microhz() const;
    uint32_t refresh_millihz() const;
    uint64_t next() const;
    uint64_t advance(uint64_t now);
    uint64_t sequence() const;
    uint64_t last() const;

    bool invariants_hold() const;

private:
    uint64_t due(uint64_t sequence) const;
    void     rebase(uint64_t sequence, uint64_t at);

    uint32_t microhz_;
    uint64_t epoch_;
    uint64_t epoch_sequence_;
    uint64_t sequence_;
    uint64_t last_;
    bool     running_;

    friend struct VblankScheduleProbe;
};

}

#endif
