#include "icd/opencl/cl_device_report.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/pci_identity.hpp"

#include <CL/cl_ext.h>

namespace av::clicd {

namespace {
constexpr uint32_t implemented_major = 3u;
constexpr uint32_t implemented_minor = 0u;

constexpr cl_device_svm_capabilities carried_svm = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;

constexpr const char *device_prefix = "OpenCL ";
constexpr const char *language_prefix = "OpenCL C ";
constexpr const char *reported_by = " avgpu";

constexpr cl_device_info pci_bus_nv = 0x4008u;
constexpr cl_device_info pci_slot_nv = 0x4009u;
constexpr cl_device_info pci_domain_nv = 0x400Au;

constexpr cl_uint topology_pcie_amd = 1u;
constexpr size_t  topology_bytes = 24u;
constexpr size_t  topology_bus_at = 21u;

}

ClDeviceReport::ClDeviceReport(uint64_t adapter_luid)
    : luid_(adapter_luid), adapter_(), version_(device_prefix, implemented_major, implemented_minor),
      c_version_(language_prefix, implemented_major, implemented_minor), version_text_{}, c_version_text_{},
      svm_(0u)
{
    AV_SCOPE("ClDeviceReport::ClDeviceReport");
    version_.write(version_text_, sizeof version_text_, reported_by);
    c_version_.write(c_version_text_, sizeof c_version_text_, reported_by);
}

Status ClDeviceReport::observe(icd::VersionText &version, char *text, const char *host, size_t host_bytes)
{
    AV_SCOPE("ClDeviceReport::observe");
    const char  *rest = nullptr;
    const Status read = version.read(host, host_bytes, rest);
    if (read != Status::ok) return read;
    version.write(text, version_bytes, reported_by);
    return Status::ok;
}

Status ClDeviceReport::observe_version(const char *host, size_t host_bytes)
{
    AV_SCOPE("ClDeviceReport::observe_version");
    return observe(version_, version_text_, host, host_bytes);
}

Status ClDeviceReport::observe_c_version(const char *host, size_t host_bytes)
{
    AV_SCOPE("ClDeviceReport::observe_c_version");
    return observe(c_version_, c_version_text_, host, host_bytes);
}

size_t ClDeviceReport::length_within(const char *text, size_t bytes) const
{
    AV_SCOPE("ClDeviceReport::length_within");
    size_t length = 0u;
    while (length < bytes && text[length] != '\0') ++length;
    return length;
}

void ClDeviceReport::observe_svm(cl_device_svm_capabilities host)
{
    AV_SCOPE("ClDeviceReport::observe_svm");
    svm_ = host & carried_svm;
}

bool ClDeviceReport::decides(cl_device_info name_asked) const
{
    AV_SCOPE("ClDeviceReport::decides");
    return info(name_asked, 0u, nullptr, nullptr) == CL_SUCCESS;
}

cl_int ClDeviceReport::answer(const void *from, size_t bytes, size_t value_bytes, void *value,
                              size_t *size_ret) const
{
    AV_SCOPE("ClDeviceReport::answer");
    if (value != nullptr) {
        if (value_bytes < bytes) return CL_INVALID_VALUE;
        const char *const source = static_cast<const char *>(from);
        char *const       out    = static_cast<char *>(value);
        for (size_t index = 0u; index < bytes; ++index) out[index] = source[index];
    }
    if (size_ret != nullptr) *size_ret = bytes;
    return CL_SUCCESS;
}

cl_int ClDeviceReport::answer_text(const char *text, size_t value_bytes, void *value,
                                   size_t *size_ret) const
{
    AV_SCOPE("ClDeviceReport::answer_text");
    return answer(text, length_within(text, version_bytes - 1u) + 1u, value_bytes, value, size_ret);
}

cl_int ClDeviceReport::info(cl_device_info name_asked, size_t value_bytes, void *value,
                            size_t *size_ret) const
{
    AV_SCOPE("ClDeviceReport::info");
    switch (name_asked) {
    case CL_DEVICE_VENDOR_ID: {
        const cl_uint reported = vendor();
        return answer(&reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_NAME: return answer_name(value_bytes, value, size_ret);
    case CL_DEVICE_PCI_BUS_INFO_KHR: {
        if (!adapter_.knows(icd::AdapterFacts::location_fact)) return CL_INVALID_VALUE;
        const PciLocation             &location = adapter_.location();
        const cl_device_pci_bus_info_khr reported = {0u, location.bus(), location.device(), location.function()};
        return answer(&reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_BOARD_NAME_AMD:
        return vendor() == PciIdentity::amd_vendor ? answer_name(value_bytes, value, size_ret) : CL_INVALID_VALUE;
    case CL_DEVICE_TOPOLOGY_AMD:
    case pci_bus_nv:
    case pci_slot_nv:
    case pci_domain_nv: return answer_vendor_location(name_asked, value_bytes, value, size_ret);
    case CL_DEVICE_LUID_KHR: {
        cl_uchar reported[CL_LUID_SIZE_KHR];
        for (size_t index = 0u; index < sizeof reported; ++index) {
            reported[index] = static_cast<cl_uchar>((luid_ >> (index * 8u)) & 0xFFu);
        }
        return answer(reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_LUID_VALID_KHR: {
        const cl_bool reported = luid_ != 0u ? CL_TRUE : CL_FALSE;
        return answer(&reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_VERSION: return answer_text(version_text_, value_bytes, value, size_ret);
    case CL_DEVICE_OPENCL_C_VERSION: return answer_text(c_version_text_, value_bytes, value, size_ret);
    case CL_DEVICE_NUMERIC_VERSION: {
        const cl_version reported = CL_MAKE_VERSION(version_.major(), version_.minor(), 0u);
        return answer(&reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_SVM_CAPABILITIES:
        return answer(&svm_, sizeof svm_, value_bytes, value, size_ret);
    case CL_DEVICE_HOST_UNIFIED_MEMORY: {
        const cl_bool reported = CL_FALSE;
        return answer(&reported, sizeof reported, value_bytes, value, size_ret);
    }
    default: return answer_not_carried(name_asked, value_bytes, value, size_ret);
    }
}

cl_int ClDeviceReport::answer_not_carried(cl_device_info name_asked, size_t value_bytes, void *value,
                                          size_t *size_ret) const
{
    AV_SCOPE("ClDeviceReport::answer_not_carried");
    switch (name_asked) {
    case CL_DEVICE_EXECUTION_CAPABILITIES: {
        const cl_device_exec_capabilities reported = CL_EXEC_KERNEL;
        return answer(&reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_PARTITION_MAX_SUB_DEVICES:
    case CL_DEVICE_MAX_ON_DEVICE_QUEUES:
    case CL_DEVICE_MAX_ON_DEVICE_EVENTS:
    case CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE:
    case CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE: {
        const cl_uint none = 0u;
        return answer(&none, sizeof none, value_bytes, value, size_ret);
    }
    case CL_DEVICE_PARTITION_PROPERTIES: {
        const cl_device_partition_property none = 0;
        return answer(&none, sizeof none, value_bytes, value, size_ret);
    }
    case CL_DEVICE_PARTITION_AFFINITY_DOMAIN: {
        const cl_device_affinity_domain none = 0u;
        return answer(&none, sizeof none, value_bytes, value, size_ret);
    }
    case CL_DEVICE_PARTITION_TYPE: return answer(nullptr, 0u, value_bytes, value, size_ret);
    case CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES: {
        const cl_command_queue_properties none = 0u;
        return answer(&none, sizeof none, value_bytes, value, size_ret);
    }
    case CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES: {
        const cl_device_device_enqueue_capabilities none = 0u;
        return answer(&none, sizeof none, value_bytes, value, size_ret);
    }
    default: return CL_INVALID_VALUE;
    }
}

cl_int ClDeviceReport::answer_name(size_t value_bytes, void *value, size_t *size_ret) const
{
    AV_SCOPE("ClDeviceReport::answer_name");
    const char *const text = adapter_.name();
    if (text[0] == '\0') return CL_INVALID_VALUE;
    return answer(text, length_within(text, icd::AdapterString::bytes - 1u) + 1u, value_bytes, value, size_ret);
}

cl_int ClDeviceReport::answer_vendor_location(cl_device_info name_asked, size_t value_bytes, void *value,
                                              size_t *size_ret) const
{
    AV_SCOPE("ClDeviceReport::answer_vendor_location");
    if (!adapter_.knows(icd::AdapterFacts::location_fact)) return CL_INVALID_VALUE;
    if (name_asked == CL_DEVICE_TOPOLOGY_AMD) {
        if (vendor() != PciIdentity::amd_vendor) return CL_INVALID_VALUE;
        cl_uchar reported[topology_bytes] = {};
        for (size_t index = 0u; index < sizeof topology_pcie_amd; ++index)
            reported[index] = static_cast<cl_uchar>((topology_pcie_amd >> (8u * index)) & 0xFFu);
        reported[topology_bus_at] = static_cast<cl_uchar>(adapter_.location().bus());
        reported[topology_bus_at + 1u] = static_cast<cl_uchar>(adapter_.location().device());
        reported[topology_bus_at + 2u] = static_cast<cl_uchar>(adapter_.location().function());
        return answer(reported, sizeof reported, value_bytes, value, size_ret);
    }
    if (vendor() != PciIdentity::nvidia_vendor) return CL_INVALID_VALUE;
    const cl_uint reported = name_asked == pci_bus_nv ? adapter_.location().bus()
                             : name_asked == pci_slot_nv ? adapter_.location().device()
                                                         : 0u;
    return answer(&reported, sizeof reported, value_bytes, value, size_ret);
}

bool ClDeviceReport::on_vendor(uint32_t wanted) const
{
    AV_SCOPE("ClDeviceReport::on_vendor");
    return vendor() == wanted;
}

uint32_t ClDeviceReport::vendor() const
{
    AV_SCOPE("ClDeviceReport::vendor");
    return adapter_.knows(icd::AdapterFacts::identity_fact) ? adapter_.identity().vendor() : PciIdentity::virtio_vendor;
}

void ClDeviceReport::observe_adapter(const icd::AdapterFacts &adapter)
{
    AV_SCOPE("ClDeviceReport::observe_adapter");
    adapter_ = adapter;
}

const char *ClDeviceReport::adapter_name() const
{
    AV_SCOPE("ClDeviceReport::adapter_name");
    return adapter_.name();
}

void ClDeviceReport::observe_luid(uint64_t adapter_luid)
{
    AV_SCOPE("ClDeviceReport::observe_luid");
    luid_ = adapter_luid;
}

bool ClDeviceReport::names_adapter(uint64_t adapter_luid) const
{
    AV_SCOPE("ClDeviceReport::names_adapter");
    return luid_ != 0u && luid_ == adapter_luid;
}

bool ClDeviceReport::invariants_hold() const
{
    AV_SCOPE("ClDeviceReport::invariants_hold");
    if (!version_.invariants_hold() || !c_version_.invariants_hold()) return false;
    if (version_text_[0] == '\0' || length_within(version_text_, version_bytes) == version_bytes) return false;
    if (c_version_text_[0] == '\0' || length_within(c_version_text_, version_bytes) == version_bytes) return false;
    return (svm_ & ~carried_svm) == 0u;
}

}
