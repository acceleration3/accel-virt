/* The OpenGL ICD's one way to the rvkgpu adapter in a process (research.md
 * §3): the adapter found by enumeration - a session-0 window's DC opens none
 * (050 T009) - and a device on it that every context's transport shares. It
 * opens with the first context made and closes with the last one gone. */
#ifndef AV_GLICD_GL_CONNECTION_HPP
#define AV_GLICD_GL_CONNECTION_HPP

#include "core/support/status.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class StateWriter;
}  // namespace av

namespace av::glicd {

class GlConnection {
public:
    explicit GlConnection(Kmt &kmt);
    ~GlConnection();
    GlConnection(const GlConnection &) = delete;
    GlConnection &operator=(const GlConnection &) = delete;

    /* One more context uses the device: the first opens it. not_supported
     * when there is no rvkgpu adapter or no device on it, with nothing left
     * open and no user counted. */
    [[nodiscard]] Status acquire();
    /* One context fewer; the last closes the device and the adapter. */
    void                 release();
    uint32_t             users() const;
    /* The device the transports open on; only while a user holds it. */
    GuestConnection     &device();
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    AdapterSearch   search_;
    GuestConnection device_;
    uint32_t        users_;

    friend struct GlConnectionProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_CONNECTION_HPP
