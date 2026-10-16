/* One device this ICD shows: one avgpu adapter (AvgpuAdapters), and the host
 * GPU the executor bound to it. Everything an application can tell one device
 * from another by lives here - which adapter its contexts are made on, what
 * Windows calls that adapter and which extensions its vendor's rows allow - so
 * two devices never answer for each other.
 *
 * A host device handle means something only on a session of the adapter that
 * answered it: each adapter's executor is bound to its own GPU and registers
 * the handles it gives out per session. So a device's contexts are all made
 * on its own connection, and the handle it is known by comes from the ICD's
 * own context on that connection. */
#ifndef AV_CLICD_CL_DEVICE_HPP
#define AV_CLICD_CL_DEVICE_HPP

#include "icd/common/icd_connection.hpp"
#include "icd/opencl/cl_device_report.hpp"
#include "icd/opencl/cl_extension_policy.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class Kmt;
class StateWriter;
}  // namespace av

namespace av::clicd {

class ClObject;
class GuestClContext;

class ClDevice {
public:
    /* On the avgpu adapter whose LUID is `luid`. */
    ClDevice(BlockSource &memory, Kmt &kmt, uint64_t luid);
    ClDevice(const ClDevice &) = delete;
    ClDevice &operator=(const ClDevice &) = delete;

    uint64_t            luid() const;
    icd::IcdConnection &connection();
    ClDeviceReport     &decided();
    ClExtensionPolicy  &extensions();

    /* The application's handle for it, once the ICD's own context on it has
     * asked the host for its GPU; null before. */
    ClObject *object() const;
    void      opened(ClObject &object);
    /* Whether `object` is this device's handle, and whether `context` was
     * made on its adapter. */
    bool      is(const ClObject &object) const;
    bool      runs(const GuestClContext &context) const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    uint64_t           luid_;
    icd::IcdConnection connection_;
    ClDeviceReport     report_;
    ClExtensionPolicy  extensions_;
    ClObject          *object_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_DEVICE_HPP
