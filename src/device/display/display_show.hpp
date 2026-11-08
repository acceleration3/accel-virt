#ifndef AV_DISPLAY_SHOW_HPP
#define AV_DISPLAY_SHOW_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/display/display_clock.hpp"
#include "device/host/host.hpp"

#include <stdint.h>

namespace av {

class DisplayShow {
public:
    DisplayShow(Host &host, HostRoutine exporter, void *context);
    ~DisplayShow();
    DisplayShow(const DisplayShow &) = delete;
    DisplayShow &operator=(const DisplayShow &) = delete;

    [[nodiscard]] Status start();
    void                 wake();
    void                 stop();
    void                 run();
    void                 hold(uint32_t head, const AvExportedImage &image);
    bool                 take(uint32_t head, AvExportedImage &image);
    bool                 running() const;

    bool invariants_hold() const;

private:
    Host            &host_;
    HostRoutine      exporter_;
    void            *context_;
    void            *lock_;
    void            *wake_;
    void            *thread_;
    bool             stopping_;
    bool             held_[DisplayClock::heads];
    AvExportedImage  images_[DisplayClock::heads];
};

}

#endif
