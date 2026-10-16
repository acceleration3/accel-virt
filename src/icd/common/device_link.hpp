/* One device on an avgpu adapter that a driver's contexts share - OpenGL's
 * and OpenCL's, through their IcdConnection - and how many hold it: it opens
 * with the first context made on it and closes with the last one gone. When
 * one context's channel is lost (ContextLink::lost), the device is lost for
 * all of them, each finding it out at its own next call, and new contexts
 * get a device of their own (LinkGenerations). */
#ifndef AV_ICD_DEVICE_LINK_HPP
#define AV_ICD_DEVICE_LINK_HPP

#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class StateWriter;
}  // namespace av

namespace av::icd {

class DeviceLink {
public:
    explicit DeviceLink(Kmt &kmt);
    /* The device closes with the link. */
    ~DeviceLink();
    DeviceLink(const DeviceLink &) = delete;
    DeviceLink &operator=(const DeviceLink &) = delete;

    /* The device made on `adapter` for its first user; GuestConnection::open's
     * answer. */
    [[nodiscard]] Status open(uint32_t adapter);
    /* One user more; one fewer, true for the last. */
    void                 hold();
    [[nodiscard]] bool   let_go();
    uint32_t             users() const;
    /* No new context joins the device. Any thread. */
    void                 lose();
    bool                 lost() const;
    GuestConnection     &device();
    const GuestConnection &device() const;

    void report(StateWriter &writer) const;
    /* A link held is a sound device open; one nobody holds is one just made. */
    bool invariants_hold() const;

private:
    GuestConnection device_;
    uint32_t        users_;
    bool            lost_;
};

}  // namespace av::icd

#endif  // AV_ICD_DEVICE_LINK_HPP
