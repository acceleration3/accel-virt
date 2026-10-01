/* An ICD's ways to the avgpu adapter in a process: the adapter found by
 * enumeration - a session-0 window's DC opens none (feature 050, T009) - and
 * the devices on it that contexts' transports share. A device opens with the
 * first context made on it and closes with the last one gone. When one
 * context's transport faults, its whole device is lost (050 US3, T040): the
 * contexts on it keep their link until they are deleted, and the next context
 * made opens a new device beside it.
 *
 * Nothing here is any one API's. It was the OpenGL ICD's `IcdConnection` until
 * the OpenCL ICD needed the same thing, which is the same thing down to the
 * loss policy: a cl_context outliving a fault has to keep answering errors
 * rather than take the process with it, exactly as a GL context does. What an
 * ICD adds on top is which sub-API its transports speak, and that is the
 * transport's business rather than the device's. */
#ifndef AV_ICD_CONNECTION_HPP
#define AV_ICD_CONNECTION_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class StateWriter;
}  // namespace av

namespace av::icd {

class IcdConnection {
public:
    /* A process that loses a device three times while contexts on each stay
     * alive has had enough devices. */
    static constexpr uint32_t max_links = 4u;
    static constexpr uint32_t no_link = max_links;

    explicit IcdConnection(Kmt &kmt);
    ~IcdConnection();
    IcdConnection(const IcdConnection &) = delete;
    IcdConnection &operator=(const IcdConnection &) = delete;

    /* One more context uses a device: the device new contexts share, or a
     * new one when there is none or it was lost. `link` names it; no_link on
     * failure. not_supported with no avgpu adapter or no device made on it,
     * no_space with every link lost and still held - nothing opened for
     * either. */
    [[nodiscard]] Status acquire(uint32_t &link);
    /* One context fewer on `link`; its last closes the device, and the last
     * device the adapter. A link not held is ignored. */
    void                 release(uint32_t link);
    /* The device behind `link` faulted: no new context joins it. A link not
     * held is ignored. */
    void                 lose(uint32_t link);
    bool                 lost(uint32_t link) const;
    /* Contexts holding any link. */
    uint32_t             users() const;
    /* The device a held link names. */
    GuestConnection     &device(uint32_t link);
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    bool held(uint32_t link) const;
    [[nodiscard]] Status open_link(uint32_t link);
    void                 close_adapter_if_idle();

    Kmt                           &kmt_;
    AdapterSearch                  search_;
    ObjectStorage<GuestConnection> devices_[max_links];
    uint32_t                       users_[max_links];
    bool                           lost_[max_links];
    /* The link new contexts join: held and not lost, or no_link. */
    uint32_t                       current_;

    friend struct IcdConnectionProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_CONNECTION_HPP
