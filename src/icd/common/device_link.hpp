#ifndef AV_ICD_DEVICE_LINK_HPP
#define AV_ICD_DEVICE_LINK_HPP

#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class StateWriter;
}

namespace av::icd {

class DeviceLink {
public:
    explicit DeviceLink(Kmt &kmt);
    ~DeviceLink();
    DeviceLink(const DeviceLink &) = delete;
    DeviceLink &operator=(const DeviceLink &) = delete;

    [[nodiscard]] Status open(uint32_t adapter);
    void                 hold();
    [[nodiscard]] bool   let_go();
    uint32_t             users() const;
    void                 lose();
    bool                 lost() const;
    GuestConnection     &device();
    const GuestConnection &device() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    GuestConnection device_;
    uint32_t        users_;
    bool            lost_;
};

}

#endif
