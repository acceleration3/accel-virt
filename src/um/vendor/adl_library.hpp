/* atiadlxx.dll's root behind its exports (adl_exports.cpp), one per process
 * in their storage. An ADL2 call names its context and an ADL1 call none,
 * which is ADL1's; a context this library did not hand out is
 * ADL_ERR_INVALID_PARAM. Translation only: what is answered is decided in
 * src/icd/vendor/adl_root.cpp. */
#ifndef AV_UM_ADL_LIBRARY_HPP
#define AV_UM_ADL_LIBRARY_HPP

#include "icd/vendor/adl_root.hpp"
#include "um/vendor/vendor_driver.hpp"

namespace av::vendor {
struct AdlAdapterInfoRecord;
struct AdlVersionsInfoRecord;
struct AdlVersionsInfoX2Record;
}  // namespace av::vendor

namespace av {

class AdlLibrary {
public:
    constexpr AdlLibrary();
    AdlLibrary(const AdlLibrary &) = delete;
    AdlLibrary &operator=(const AdlLibrary &) = delete;

    /* `context`, when not null, receives the ADL2 context. */
    int create(bool has_callback, void **context);
    int destroy(const void *context);
    int refresh(const void *context);
    int number_of_adapters(const void *context, int *count);
    int adapter_info(const void *context, vendor::AdlAdapterInfoRecord *infos, int bytes);
    int active(const void *context, int index, int *status);
    int asic_family_type(const void *context, int index, int *types, int *valids);
    int primary(const void *context, int *index);
    int versions(const void *context, vendor::AdlVersionsInfoRecord *versions);
    int versions_x2(const void *context, vendor::AdlVersionsInfoX2Record *versions);
    /* ADL2_Main_Control_GetProcAddress: a call of this module's, resolved for
     * a caller holding `context`; null for another context or another name. */
    void *proc_address(const void *context, const char *name);

private:
    /* The root a call naming `context` reaches, or null. */
    vendor::AdlRoot *root(const void *context);

    VendorDriver<vendor::AdlRoot> driver_;
};

constexpr AdlLibrary::AdlLibrary() : driver_() {}

}  // namespace av

#endif  // AV_UM_ADL_LIBRARY_HPP
