#ifndef AV_WINDOW_STATS_WINDOW_HPP
#define AV_WINDOW_STATS_WINDOW_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class StateWriter;
class WindowStats;

class WindowStatsWindow {
public:
    static constexpr uint32_t read_attempts = 16u;

    explicit WindowStatsWindow(Host &host);
    ~WindowStatsWindow();
    WindowStatsWindow(const WindowStatsWindow &) = delete;
    WindowStatsWindow &operator=(const WindowStatsWindow &) = delete;

    [[nodiscard]] Status map(uint64_t address);
    void                 release();
    bool                 mapped() const;
    [[nodiscard]] Status read(WindowStats &stats) const;
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    Host    &host_;
    uint8_t *page_;

    friend struct WindowStatsWindowProbe;
};

}

#endif
