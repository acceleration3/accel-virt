#ifndef AV_DEVICE_PULSE_HPP
#define AV_DEVICE_PULSE_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class FenceReporter;
class Host;
struct DevicePulseProbe;

class DevicePulse {
public:
    static constexpr uint32_t capacity = 64u;
    static constexpr uint64_t period_ns = 250000000u;

    explicit DevicePulse(Host &host);
    ~DevicePulse();
    DevicePulse(const DevicePulse &) = delete;
    DevicePulse &operator=(const DevicePulse &) = delete;

    [[nodiscard]] Status initialize();
    [[nodiscard]] Status carry(FenceReporter &reporter);
    void                 drop(FenceReporter &reporter);
    uint32_t             carried() const;
    void                 stir();
    void                 note_completion();
    uint64_t             completions() const;
    void                 run();
    bool                 tick(bool &working);
    bool                 rest();

    bool invariants_hold() const;

private:
    void stop_thread();
    bool any_working() const;

    Host          &host_;
    void          *lock_;
    void          *wake_;
    void          *thread_;
    bool           stopping_;
    bool           dormant_;
    uint64_t       completions_;
    FenceReporter *carried_[capacity];

    friend struct DevicePulseProbe;
};

}

#endif
