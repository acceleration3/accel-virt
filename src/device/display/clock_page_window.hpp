#ifndef AV_CLOCK_PAGE_WINDOW_HPP
#define AV_CLOCK_PAGE_WINDOW_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class VblankRecord;

class ClockPageWindow {
public:
    explicit ClockPageWindow(Host &host);
    ~ClockPageWindow();
    ClockPageWindow(const ClockPageWindow &) = delete;
    ClockPageWindow &operator=(const ClockPageWindow &) = delete;

    [[nodiscard]] Status map(uint64_t address, uint32_t heads);
    void                 release();
    bool                 mapped() const;
    void                 publish(uint32_t head, const VblankRecord &blank);
    uint32_t             interrupt_every(uint32_t head) const;

    bool invariants_hold() const;

private:
    uint32_t *word(uint64_t offset) const;

    Host    &host_;
    uint8_t *page_;
    uint32_t heads_;

    friend struct ClockPageWindowProbe;
};

}

#endif
