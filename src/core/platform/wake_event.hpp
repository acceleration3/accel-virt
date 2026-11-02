#ifndef AV_WAKE_EVENT_HPP
#define AV_WAKE_EVENT_HPP

#include "core/platform/platform.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WakeEvent {
public:
    static constexpr size_t storage_bytes = 32u;

    explicit WakeEvent(Platform &platform);
    WakeEvent(const WakeEvent &) = delete;
    WakeEvent &operator=(const WakeEvent &) = delete;

    void               set();
    [[nodiscard]] bool wait(uint64_t ticks);

private:
    Platform &platform_;
    alignas(8) unsigned char storage_[storage_bytes];
};

}

#endif
