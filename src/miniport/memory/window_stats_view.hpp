#ifndef AV_WINDOW_STATS_VIEW_HPP
#define AV_WINDOW_STATS_VIEW_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class WindowStats;

class WindowStatsView {
public:
    explicit WindowStatsView(Platform &platform);
    ~WindowStatsView();
    WindowStatsView(const WindowStatsView &) = delete;
    WindowStatsView &operator=(const WindowStatsView &) = delete;

    [[nodiscard]] Status initialize();
    bool                 ready() const;
    uint64_t             address() const;
    void                 publish(const WindowStats &stats);

    bool invariants_hold() const;

private:
    uint32_t *sequence() const;
    uint64_t *field(uint32_t index) const;

    Platform &platform_;
    uint8_t  *page_;
};

}

#endif
