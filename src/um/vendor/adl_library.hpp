#ifndef AV_UM_ADL_LIBRARY_HPP
#define AV_UM_ADL_LIBRARY_HPP

#include "icd/vendor/adl_root.hpp"
#include "um/vendor/vendor_driver.hpp"

namespace av::vendor {
struct AdlAdapterInfoRecord;
struct AdlVersionsInfoRecord;
struct AdlVersionsInfoX2Record;
}

namespace av {

class AdlLibrary {
public:
    constexpr AdlLibrary();
    AdlLibrary(const AdlLibrary &) = delete;
    AdlLibrary &operator=(const AdlLibrary &) = delete;

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
    void *proc_address(const void *context, const char *name);

private:
    vendor::AdlRoot *root(const void *context);

    VendorDriver<vendor::AdlRoot> driver_;
};

constexpr AdlLibrary::AdlLibrary() : driver_() {}

}

#endif
