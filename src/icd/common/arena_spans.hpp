#ifndef AV_ICD_ARENA_SPANS_HPP
#define AV_ICD_ARENA_SPANS_HPP

#include <stdint.h>

namespace av::icd {

class HostChannel;

class ArenaSpans {
public:
    static constexpr uint64_t page_bytes = 4096u;

    explicit ArenaSpans(HostChannel &channel);
    ArenaSpans(const ArenaSpans &) = delete;
    ArenaSpans &operator=(const ArenaSpans &) = delete;

    [[nodiscard]] bool slots(uint32_t count, uint8_t *&view, uint64_t &offset);
    [[nodiscard]] bool span(uint64_t bytes, uint8_t *&view, uint64_t &length);
    uint64_t           lent() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] bool set_aside(uint64_t start, uint8_t *&view);

    HostChannel &channel_;
    uint64_t     low_;
};

}

#endif
