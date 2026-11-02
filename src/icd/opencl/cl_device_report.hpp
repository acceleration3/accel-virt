#ifndef AV_CLICD_CL_DEVICE_REPORT_HPP
#define AV_CLICD_CL_DEVICE_REPORT_HPP

#include "core/support/status.hpp"

#include <CL/cl.h>

#include "guest/kmt/pci_location.hpp"
#include "icd/common/adapter_facts.hpp"
#include "icd/common/version_text.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class PciIdentity;
}

namespace av::clicd {

struct ClDeviceReportProbe;

class ClDeviceReport {
public:
    explicit ClDeviceReport(uint64_t adapter_luid);
    void     observe_luid(uint64_t adapter_luid);
    bool     names_adapter(uint64_t adapter_luid) const;
    bool     on_vendor(uint32_t wanted) const;
    void     observe_adapter(const icd::AdapterFacts &adapter);
    const char *adapter_name() const;
    ClDeviceReport(const ClDeviceReport &) = delete;
    ClDeviceReport &operator=(const ClDeviceReport &) = delete;

    bool decides(cl_device_info name_asked) const;

    cl_int info(cl_device_info name_asked, size_t value_bytes, void *value, size_t *size_ret) const;

    [[nodiscard]] Status observe_version(const char *host, size_t host_bytes);
    [[nodiscard]] Status observe_c_version(const char *host, size_t host_bytes);

    void observe_svm(cl_device_svm_capabilities host);

    bool invariants_hold() const;

private:
    friend struct ClDeviceReportProbe;

    static constexpr size_t version_bytes = 32u;

    [[nodiscard]] Status observe(icd::VersionText &version, char *text, const char *host, size_t host_bytes);
    size_t length_within(const char *text, size_t bytes) const;
    cl_int answer(const void *from, size_t bytes, size_t value_bytes, void *value,
                  size_t *size_ret) const;
    cl_int answer_text(const char *text, size_t value_bytes, void *value, size_t *size_ret) const;
    cl_int answer_name(size_t value_bytes, void *value, size_t *size_ret) const;
    cl_int answer_vendor_location(cl_device_info name_asked, size_t value_bytes, void *value,
                                  size_t *size_ret) const;
    cl_int answer_not_carried(cl_device_info name_asked, size_t value_bytes, void *value,
                              size_t *size_ret) const;
    uint32_t vendor() const;

    uint64_t                   luid_;
    icd::AdapterFacts          adapter_;
    icd::VersionText           version_;
    icd::VersionText           c_version_;
    char                       version_text_[version_bytes];
    char                       c_version_text_[version_bytes];
    cl_device_svm_capabilities svm_;
};

}

#endif
