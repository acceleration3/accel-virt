/* The clGetDeviceInfo queries this ICD answers instead of forwarding.
 *
 * Almost every device query is the host device's to answer: it is the GPU that
 * will run the work. Four are not, and each for its own reason.
 *
 * CL_DEVICE_VENDOR_ID and CL_DEVICE_LUID_KHR name an adapter. The host's name
 * the host's own GPU, which the guest has no handle to, cannot share memory
 * with, and cannot match a DXGI adapter against; the guest's adapter is
 * avgpu's. CL_DEVICE_UUID_KHR is left to the host, as the Vulkan ICD leaves
 * VkPhysicalDeviceIDProperties::deviceUUID: it identifies the physical GPU
 * doing the work, and there is no second GPU here to confuse it with.
 *
 * The versions say what an application may call, and the host's say what the
 * host implements rather than what this ICD carries, so both are capped at
 * OpenCL 3.0 (constitution VI: what is reported is what is tested).
 *
 * CL_DEVICE_SVM_CAPABILITIES says what a kernel may dereference. Feature 052
 * mirrors a coarse-grained buffer and nothing else, so a fine-grained or
 * system report would have a kernel dereference a host address the guest never
 * mirrored.
 *
 * CL_DEVICE_EXTENSIONS is neither this report's answer nor the host's:
 * ClExtensionPolicy intersects the two, and the caller asks it. */
#ifndef AV_CLICD_CL_DEVICE_REPORT_HPP
#define AV_CLICD_CL_DEVICE_REPORT_HPP

#include "core/support/status.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

struct ClDeviceReportProbe;

class ClDeviceReport {
public:
    /* Red Hat's PCI vendor, which is the virtio function avgpu drives
     * (1af4:1050). An application matching this device against an adapter it
     * enumerated matches the guest's, so this is the guest's. */
    static constexpr uint32_t vendor_id = 0x1AF4u;

    /* `adapter_luid` is the avgpu adapter's, as Kmt::adapter_luid answers it;
     * zero is what it leaves when there was no adapter, and is reported as a
     * LUID that is not valid. */
    explicit ClDeviceReport(uint64_t adapter_luid);
    /* The adapter this device is on, once the ICD has opened one: a report
     * built before there was an adapter has nothing to name. */
    void     observe_luid(uint64_t adapter_luid);
    ClDeviceReport(const ClDeviceReport &) = delete;
    ClDeviceReport &operator=(const ClDeviceReport &) = delete;

    /* Whether this answers `name_asked` at all. Everything else the caller
     * forwards to the host byte for byte. */
    bool decides(cl_device_info name_asked) const;

    /* clGetDeviceInfo for the queries it decides, in the API's own shape: the
     * value's bytes are written when `value` is not null and `value_bytes` is
     * at least the value's size, and the size is written to `size_ret` when
     * that is not null. CL_INVALID_VALUE for a query it does not decide and
     * for a buffer too small, which writes nothing. */
    cl_int info(cl_device_info name_asked, size_t value_bytes, void *value, size_t *size_ret) const;

    /* The host device's CL_DEVICE_VERSION - "OpenCL <major>.<minor> ..." - and
     * its CL_DEVICE_OPENCL_C_VERSION, each capped at what this ICD implements.
     * `host` is the host's own bytes and need not be terminated within
     * `host_bytes`; Status::malformed for anything that is not the shape the
     * specification fixes, and the report keeps the answer it had. */
    [[nodiscard]] Status observe_version(const char *host, size_t host_bytes);
    [[nodiscard]] Status observe_c_version(const char *host, size_t host_bytes);

    /* The host device's CL_DEVICE_SVM_CAPABILITIES, of which only the
     * coarse-grained buffer survives. */
    void observe_svm(cl_device_svm_capabilities host);

    bool invariants_hold() const;

private:
    friend struct ClDeviceReportProbe;

    /* "OpenCL C 3.0 avgpu" and its terminator, with room to spare. */
    static constexpr size_t version_bytes = 32u;

    cl_int answer(const void *from, size_t bytes, size_t value_bytes, void *value,
                  size_t *size_ret) const;
    cl_int answer_text(const char *text, size_t value_bytes, void *value, size_t *size_ret) const;

    uint64_t                   luid_;
    char                       version_[version_bytes];
    char                       c_version_[version_bytes];
    uint32_t                   major_;
    uint32_t                   minor_;
    uint32_t                   c_major_;
    uint32_t                   c_minor_;
    cl_device_svm_capabilities svm_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_DEVICE_REPORT_HPP
