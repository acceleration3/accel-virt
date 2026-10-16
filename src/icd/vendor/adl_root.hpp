/* atiadlxx.dll's decisions: the AMD Display Library calls an application
 * makes to find an AMD GPU, answered for the avgpu adapters that present an
 * AMD GPU (docs/vendor-libraries.md). Each returns an ADL status code.
 *
 * Only what can be said truthfully is here: the adapters, where they sit, what
 * they are called, whether integrated, which one the primary display is on,
 * and the driver package's version. Everything else ADL offers - overclocking,
 * display configuration, power - is not exported at all, which is what an
 * application already meets on a driver that predates the call. With no AMD
 * adapter the library will not initialise, as ADL does where no AMD driver
 * runs, so an application takes the path it takes on any other machine.
 *
 * ADL and ADL2 share this one state: an ADL2 context is the root itself
 * (context()), and each create - of either - is undone by one destroy. */
#ifndef AV_VENDOR_ADL_ROOT_HPP
#define AV_VENDOR_ADL_ROOT_HPP

#include "icd/vendor/vendor_adapters.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class ThreadLock;
}  // namespace av

namespace av::vendor {

struct AdlAdapterInfoRecord;
struct AdlVersionsInfoRecord;
struct AdlVersionsInfoX2Record;
class VendorAdapter;

class AdlRoot {
public:
    /* adl_defines.h's status codes this library answers. */
    static constexpr int ok = 0;
    static constexpr int error = -1;
    static constexpr int not_initialized = -2;
    static constexpr int invalid_parameter = -3;
    static constexpr int invalid_parameter_size = -4;
    static constexpr int invalid_adapter_index = -5;
    static constexpr int null_pointer = -9;
    static constexpr int invalid_callback = -11;
    /* ADL_ASIC_DISCRETE and ADL_ASIC_INTEGRATED. */
    static constexpr int asic_discrete = 1;
    static constexpr int asic_integrated = 2;

    /* `lock` is what keeps two threads' calls apart. */
    AdlRoot(Kmt &kmt, ThreadLock &lock);
    AdlRoot(const AdlRoot &) = delete;
    AdlRoot &operator=(const AdlRoot &) = delete;

    /* ADL_Main_Control_Create and ADL2_: learns the adapters. A missing
     * allocation callback is invalid_callback, as ADL's own; no AMD adapter is
     * error, the answer where no AMD driver runs. */
    int create(bool has_callback);
    /* ADL_Main_Control_Destroy and ADL2_: one create undone. */
    int destroy();
    /* ADL_Main_Control_Refresh and ADL2_: learns the adapters again. */
    int refresh();
    /* What an ADL2 caller holds, and whether a context it passes is it - a
     * null one being ADL1's, which is this same state. */
    void *context();
    bool  holds(const void *context) const;

    int number_of_adapters(int *count);
    /* Every adapter's AdapterInfo into `infos`, which holds `bytes`. */
    int adapter_info(AdlAdapterInfoRecord *infos, int bytes);
    int active(int index, int *status);
    int asic_family_type(int index, int *types, int *valids);
    /* The index of the adapter the primary display is on; error when it is
     * on none of them. */
    int primary(int *index);
    /* The driver package's version; the Catalyst and Crimson versions and
     * the web link are empty, because this is neither driver. */
    int versions(AdlVersionsInfoRecord *versions);
    int versions_x2(AdlVersionsInfoX2Record *versions);

private:
    /* The adapter `index`, or null when uninitialised or past them. */
    const VendorAdapter *adapter(int index) const;
    /* The AdapterInfo of adapter `index`, which is one. */
    void describe(uint32_t index, AdlAdapterInfoRecord &info) const;
    /* The driver package's version into `out` of `capacity`: one package
     * serves every adapter, so the first's. */
    void driver_version(char *out, uint32_t capacity) const;

    ThreadLock      &lock_;
    VendorAdapters   adapters_;
    uint32_t         opened_;
};

}  // namespace av::vendor

#endif  // AV_VENDOR_ADL_ROOT_HPP
