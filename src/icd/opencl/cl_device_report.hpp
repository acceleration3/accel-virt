/* The clGetDeviceInfo queries this ICD answers instead of forwarding.
 *
 * Almost every device query is the host device's to answer: it is the GPU that
 * will run the work. These are not, and each for its own reason.
 *
 * CL_DEVICE_VENDOR_ID, CL_DEVICE_LUID_KHR, CL_DEVICE_NAME and the PCI location
 * (cl_khr_pci_bus_info, and the AMD and NVIDIA attribute queries an
 * application finds a GPU's place on the bus by) name an adapter. The host's
 * name the host's own GPU, which the guest has no handle to, cannot share
 * memory with, and cannot match a DXGI adapter or a vendor library's GPU
 * against; the guest's adapter is avgpu's, and these are what Windows says of
 * it (docs/vendor-libraries.md). CL_DEVICE_UUID_KHR is left to the host, as the Vulkan ICD leaves
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
 * CL_DEVICE_HOST_UNIFIED_MEMORY says whether the application's memory is the
 * device's. The host's says whether the host's is - yes for an integrated
 * GPU - but the application's memory here is the guest's, which no host GPU
 * reads or writes: a buffer over it is a copy (ClIcdRoot::create_buffer). An
 * application told yes takes the zero-copy path and reads a kernel's results
 * out of its own memory without a map, which is what the API allows a
 * unified device to be relied on for in practice; Affinity does, and on an
 * iGPU its canvas never saw a tile it rendered. So the answer is no, whatever
 * the GPU, and the application maps or reads.
 *
 * Some of what a host GPU can do no guest of this ICD can reach, and those
 * are answered as absent whatever the host says, so an application that
 * checks before it calls is not sent to a refusal: a native kernel (a guest
 * function the host would have to call), partitions into sub-devices (one
 * device per adapter is all a context here spans) and device-side enqueue
 * (no queue here is a device queue). Each has its entry points refused with
 * the error the specification gives a device without the feature.
 *
 * CL_DEVICE_EXTENSIONS is neither this report's answer nor the host's:
 * ClExtensionPolicy intersects the two, and the caller asks it. */
#ifndef AV_CLICD_CL_DEVICE_REPORT_HPP
#define AV_CLICD_CL_DEVICE_REPORT_HPP

#include "core/support/status.hpp"

#include <CL/cl.h>

#include "guest/kmt/pci_location.hpp"
#include "icd/common/adapter_facts.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class PciIdentity;
}  // namespace av

namespace av::clicd {

struct ClDeviceReportProbe;

class ClDeviceReport {
public:
    /* `adapter_luid` is the avgpu adapter's, as Kmt::adapter_luid answers it;
     * zero is what it leaves when there was no adapter, and is reported as a
     * LUID that is not valid. */
    explicit ClDeviceReport(uint64_t adapter_luid);
    /* The adapter this device is on, once the ICD has opened one: a report
     * built before there was an adapter has nothing to name. */
    void     observe_luid(uint64_t adapter_luid);
    /* Whether `adapter_luid` is the adapter this device is on; a report with
     * no adapter names none. */
    bool     names_adapter(uint64_t adapter_luid) const;
    /* Whether the adapter presents `wanted`'s GPU (a PCI vendor). */
    bool     on_vendor(uint32_t wanted) const;
    /* What Windows says of that adapter: its PCI identity, where it sits
     * and its name, each as far as Windows gave it. */
    void     observe_adapter(const icd::AdapterFacts &adapter);
    /* That adapter's name, empty until one is observed with a name. */
    const char *adapter_name() const;
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
    /* The adapter's name, or CL_INVALID_VALUE where it has none. */
    cl_int answer_name(size_t value_bytes, void *value, size_t *size_ret) const;
    /* The AMD and NVIDIA queries of where the device sits, on an adapter of
     * that vendor; CL_INVALID_VALUE otherwise. */
    cl_int answer_vendor_location(cl_device_info name_asked, size_t value_bytes, void *value,
                                  size_t *size_ret) const;
    /* The features no guest can reach, as a device without them answers;
     * CL_INVALID_VALUE for any other query. */
    cl_int answer_not_carried(cl_device_info name_asked, size_t value_bytes, void *value,
                              size_t *size_ret) const;
    /* The PCI vendor answered: Red Hat's, the virtio function avgpu drives
     * (1af4:1050), before an adapter is described; once one is, its own -
     * the host GPU's vendor, which avgpu presents. */
    uint32_t vendor() const;

    uint64_t                   luid_;
    icd::AdapterFacts          adapter_;
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
