#ifndef AV_CLICD_CL_DEVICES_HPP
#define AV_CLICD_CL_DEVICES_HPP

#include "guest/kmt/avgpu_adapters.hpp"
#include "core/support/object_storage.hpp"
#include "icd/opencl/cl_device.hpp"

#include <stdint.h>

namespace av {
class AvgpuAdapters;
class BlockSource;
class Kmt;
class StateWriter;
}

namespace av::clicd {

class ClObject;
class GuestClContext;

struct ClDevicesProbe;

class ClDevices {
public:
    static constexpr uint32_t max_devices = AvgpuAdapters::max_adapters;

    ClDevices(BlockSource &memory, Kmt &kmt);
    ~ClDevices();
    ClDevices(const ClDevices &) = delete;
    ClDevices &operator=(const ClDevices &) = delete;

    void      learn(const AvgpuAdapters &adapters);
    bool      learnt() const;
    uint32_t  count() const;
    uint32_t  left_out() const;
    ClDevice &at(uint32_t index);
    ClDevice *of(const ClObject &object);
    ClDevice *running(const GuestClContext &context);
    ClDevice *on(uint64_t luid);

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    friend struct ClDevicesProbe;

    BlockSource &memory_;
    Kmt                    &kmt_;
    ObjectStorage<ClDevice> devices_[max_devices];
    uint32_t                count_;
    uint32_t                left_out_;
    bool                    learnt_;
};

}

#endif
