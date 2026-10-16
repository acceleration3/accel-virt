/* An ICD's ways to the avgpu adapter in a process: the adapter found by
 * enumeration - a session-0 window's DC opens none (feature 050, T009) - and
 * the devices on it that contexts' transports share (DeviceLink), one
 * generation at a time (LinkGenerations): a device opens with the first
 * context made on it and closes with the last one gone, and when one
 * context's transport faults its whole device is lost (050 US3, T040): the
 * contexts on it keep it until they are deleted, and the next context made
 * opens a new device beside it.
 *
 * Nothing here is any one API's. It was the OpenGL ICD's `IcdConnection` until
 * the OpenCL ICD needed the same thing, which is the same thing down to the
 * loss policy: a cl_context outliving a fault has to keep answering errors
 * rather than take the process with it, exactly as a GL context does. What an
 * ICD adds on top is which sub-API its transports speak, and that is the
 * transport's business rather than the device's. */
#ifndef AV_ICD_CONNECTION_HPP
#define AV_ICD_CONNECTION_HPP

#include "core/support/status.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "icd/common/device_link.hpp"
#include "icd/common/link_generations.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class Kmt;
class StateWriter;
}  // namespace av

namespace av::icd {

class IcdConnection {
public:
    /* On the avgpu adapter whose LUID is `luid`; 0 takes the one the primary
     * display is on (AdapterSearch::find_avgpu). */
    IcdConnection(BlockSource &memory, Kmt &kmt, uint64_t luid);
    IcdConnection(const IcdConnection &) = delete;
    IcdConnection &operator=(const IcdConnection &) = delete;

    /* One more context uses a device: the device new contexts share, or a
     * new one when there is none or it was lost. Null in `link` on failure:
     * not_supported with no avgpu adapter or no device made on it, no_memory
     * when there is no memory for a new link - nothing opened for either. */
    [[nodiscard]] Status acquire(DeviceLink *&link);
    /* One context fewer on `link`; its last closes the device, and the last
     * device the adapter. */
    void                 release(DeviceLink &link);
    /* The adapter's handle while any link is held, else 0. */
    uint32_t             adapter() const;
    /* Contexts holding any link. */
    uint32_t             users() const;
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    void close_adapter_if_idle();

    Kmt                         &kmt_;
    uint64_t                     luid_;
    AdapterSearch                search_;
    /* After the search: a device closes before its adapter does. */
    LinkGenerations<DeviceLink>  links_;

    friend struct IcdConnectionProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_CONNECTION_HPP
