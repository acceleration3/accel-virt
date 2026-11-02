#ifndef AV_COUNTED_WAKE_SIGNAL_HPP
#define AV_COUNTED_WAKE_SIGNAL_HPP

#include "icd/opengl/wake_signal.hpp"

#include <cstdint>

namespace av {

class CountedWakeSignal final : public icd::WakeSignal {
public:
    CountedWakeSignal();

    void raise() override;
    bool wait(std::uint32_t milliseconds) override;

    std::uint32_t raises() const;
    std::uint32_t waits() const;
    std::uint32_t last_wait() const;

private:
    std::uint32_t raises_;
    std::uint32_t waits_;
    std::uint32_t last_wait_;
    bool          pending_;
};

}

#endif
