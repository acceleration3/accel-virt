#ifndef AV_GUEST_CONNECTION_HPP
#define AV_GUEST_CONNECTION_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Kmt;
struct GuestConnectionProbe;

class GuestConnection {
public:
    explicit GuestConnection(Kmt &kmt);
    GuestConnection(const GuestConnection &) = delete;
    GuestConnection &operator=(const GuestConnection &) = delete;

    [[nodiscard]] Status open(uint32_t adapter);
    void                 close();

    bool     opened() const;
    Kmt     &kmt() const;
    uint32_t adapter() const;
    uint32_t device() const;
    uint32_t queue() const;
    uint32_t fence_object() const;

    bool invariants_hold() const;

private:
    Kmt      &kmt_;
    uint32_t  adapter_;
    uint32_t  device_;
    uint32_t  queue_;
    uint32_t  fence_object_;

    friend struct GuestConnectionProbe;
};

}

#endif
