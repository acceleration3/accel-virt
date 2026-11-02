#ifndef AV_CLOCK_PAGE_VIEW_HPP
#define AV_CLOCK_PAGE_VIEW_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class VblankRecord;

class ClockPageView {
public:
    static constexpr uint32_t read_attempts = 64u;

    explicit ClockPageView(Platform &platform);
    ~ClockPageView();
    ClockPageView(const ClockPageView &) = delete;
    ClockPageView &operator=(const ClockPageView &) = delete;

    [[nodiscard]] Status initialize();
    void                 release();
    uint64_t             address() const;
    [[nodiscard]] Status read(uint32_t head, VblankRecord &blank) const;
    void                 ask(uint32_t head, uint32_t every);

    bool invariants_hold() const;

private:
    uint32_t *word(uint64_t offset) const;
    uint32_t  heads() const;

    Platform &platform_;
    uint8_t  *page_;
};

}

#endif
