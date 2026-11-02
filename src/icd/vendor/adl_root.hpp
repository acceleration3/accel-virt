#ifndef AV_VENDOR_ADL_ROOT_HPP
#define AV_VENDOR_ADL_ROOT_HPP

#include "icd/vendor/vendor_adapters.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class ThreadLock;
}

namespace av::vendor {

struct AdlAdapterInfoRecord;
struct AdlVersionsInfoRecord;
struct AdlVersionsInfoX2Record;
class VendorAdapter;

class AdlRoot {
public:
    static constexpr int ok = 0;
    static constexpr int error = -1;
    static constexpr int not_initialized = -2;
    static constexpr int invalid_parameter = -3;
    static constexpr int invalid_parameter_size = -4;
    static constexpr int invalid_adapter_index = -5;
    static constexpr int null_pointer = -9;
    static constexpr int invalid_callback = -11;
    static constexpr int asic_discrete = 1;
    static constexpr int asic_integrated = 2;

    AdlRoot(Kmt &kmt, ThreadLock &lock);
    AdlRoot(const AdlRoot &) = delete;
    AdlRoot &operator=(const AdlRoot &) = delete;

    int create(bool has_callback);
    int destroy();
    int refresh();
    void *context();
    bool  holds(const void *context) const;

    int number_of_adapters(int *count);
    int adapter_info(AdlAdapterInfoRecord *infos, int bytes);
    int active(int index, int *status);
    int asic_family_type(int index, int *types, int *valids);
    int primary(int *index);
    int versions(AdlVersionsInfoRecord *versions);
    int versions_x2(AdlVersionsInfoX2Record *versions);

private:
    const VendorAdapter *adapter(int index) const;
    void describe(uint32_t index, AdlAdapterInfoRecord &info) const;
    void driver_version(char *out, uint32_t capacity) const;

    ThreadLock      &lock_;
    VendorAdapters   adapters_;
    uint32_t         opened_;
};

}

#endif
