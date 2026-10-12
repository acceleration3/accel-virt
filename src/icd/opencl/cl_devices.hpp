/* The devices avcl shows: one ClDevice per avgpu adapter, in AvgpuAdapters'
 * order, made once and kept for the process. Torn down after every context,
 * which holds a link on its device's connection: the root declares this
 * before its context pool for that. */
#ifndef AV_CLICD_CL_DEVICES_HPP
#define AV_CLICD_CL_DEVICES_HPP

#include "core/support/object_storage.hpp"
#include "icd/opencl/cl_device.hpp"

#include <stdint.h>

namespace av {
class AvgpuAdapters;
class Kmt;
class StateWriter;
}  // namespace av

namespace av::clicd {

class ClObject;
class GuestClContext;

class ClDevices {
public:
    /* One per avgpu adapter, of which a guest has one per host GPU. */
    static constexpr uint32_t max_devices = 8u;

    explicit ClDevices(Kmt &kmt);
    ~ClDevices();
    ClDevices(const ClDevices &) = delete;
    ClDevices &operator=(const ClDevices &) = delete;

    /* A device for each adapter `adapters` lists, in its order; once. An
     * adapter Windows gave no LUID, or one past max_devices, is left out. */
    void      learn(const AvgpuAdapters &adapters);
    bool      learnt() const;
    uint32_t  count() const;
    /* Adapters learn() found and made no device for. */
    uint32_t  left_out() const;
    ClDevice &at(uint32_t index);
    /* The device `object` is the handle of, the one `context` is made on, and
     * the one on the adapter of `luid`; null for none. */
    ClDevice *of(const ClObject &object);
    ClDevice *running(const GuestClContext &context);
    ClDevice *on(uint64_t luid);

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    Kmt                    &kmt_;
    ObjectStorage<ClDevice> devices_[max_devices];
    uint32_t                count_;
    uint32_t                left_out_;
    bool                    learnt_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_DEVICES_HPP
