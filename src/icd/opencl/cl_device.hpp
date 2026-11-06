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
}

namespace av::clicd {

class ClObject;
class GuestClContext;

struct ClDeviceProbe;

class ClDevice {
public:
    ClDevice(BlockSource &memory, Kmt &kmt, uint64_t luid);
    ClDevice(const ClDevice &) = delete;
    ClDevice &operator=(const ClDevice &) = delete;

    uint64_t            luid() const;
    icd::IcdConnection &connection();
    ClDeviceReport     &decided();
    ClExtensionPolicy  &extensions();

    ClObject *object() const;
    void      opened(ClObject &object);
    bool      is(const ClObject &object) const;
    bool      runs(const GuestClContext &context) const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    friend struct ClDeviceProbe;

    uint64_t           luid_;
    icd::IcdConnection connection_;
    ClDeviceReport     report_;
    ClExtensionPolicy  extensions_;
    ClObject          *object_;
};

}

#endif
