#include "icd/opencl/cl_extension_policy.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

namespace {

size_t length_within(const char *text, size_t bytes)
{
    AV_SCOPE("length_within");
    size_t length = 0u;
    while (length < bytes && text[length] != '\0') ++length;
    return length;
}

bool same_word(const char *text, const char *name, size_t length)
{
    AV_SCOPE("same_word");
    for (size_t index = 0u; index < length; ++index) {
        if (name[index] == '\0' || text[index] != name[index]) return false;
    }
    return name[length] == '\0';
}

}  // namespace

/* The extensions this ICD reports, each with the test that proves it through
 * the ICD (tools/check_extension_policy.py).
 *
 * Two names are deliberately absent.
 *
 * `cl_khr_external_memory` and `cl_khr_external_memory_win32` are how an
 * application names memory another API made: the Win32 handle travels in
 * `clCreateBufferWithProperties`' property list, `ClExternalMemory` turns it
 * into the object behind it and opens that on the device, and the extension's
 * acquire and release are answered by `ClIcdRoot::enqueue_external` - the
 * object was acquired on the host when the device opened it and stays
 * acquired for its life. Both are answered over the host's `cl_khr_gl_sharing`
 * rather than over an external-memory extension of its own: no OpenCL on a
 * Linux host has a Win32 handle to take, and the device reaches the descriptor
 * through OpenGL instead (research.md §18 and §21). A host that cannot really
 * make a GL-sharing context is a host whose device strikes that name out of
 * what the guest is shown, so neither row is reported there.
 *
 * `cl_khr_icd` is the loader's own contract rather than something an
 * application enables, and this ICD answers its two queries without claiming
 * the extension.
 *
 * `cl_khr_d3d10_sharing` is answered here alone, so it is reported whatever
 * the host offers: which device an application's DXGI adapter or Direct3D 10
 * device is on is a question about the guest's adapters, answered by the
 * adapter's LUID, and no context is ever made from a Direct3D 10 device - the
 * device's surfaces are not the host's to name - so the rest of the extension
 * is its refusals (ClIcdRoot). Affinity looks its OpenCL device up this way
 * and uses none that cannot be.
 *
 * `cl_khr_byte_addressable_store` is a compiler's promise - a kernel may
 * store a char without touching the bytes beside it - core since OpenCL 1.1
 * and listed by every driver still. Nothing crosses the wire for it but the
 * kernels, so it is reported where the host's device lists it; DaVinci Resolve
 * logs an error for a device that does not.
 *
 * `cl_khr_d3d11_sharing` is answered here alone too, so it is reported
 * whatever the host offers: a context made from an application's Direct3D 11
 * device keeps the device, each cl_mem over one of its resources is an
 * ordinary object of the host's, and acquire and release copy the resource's
 * bytes in and back through that device (docs/d3d11-sharing.md). DaVinci
 * Resolve makes its context this way and waits forever when it cannot.
 *
 * `cl_khr_gl_sharing` is reported where the host's device lists it - which
 * it does exactly when its OpenCL made a context against a GL context of the
 * device's own. The context this ICD makes from a GL context is a context of
 * the device's made in that GL context's share group: the OpenGL ICD names
 * the context by its ring (avGetShareName), the miniport resolves the name
 * for a context of the same process, and the executor makes the host's
 * context against a GL context sharing the guest one's objects. *
 * `cl_khr_pci_bus_info`, `cl_amd_device_attribute_query` and
 * `cl_nv_device_attribute_query` say where the device sits on the PCI bus,
 * which is how an application pairs it with a DXGI adapter or a vendor
 * library's GPU (docs/vendor-libraries.md; DaVinci Resolve does). ClDeviceReport
 * answers them from what Windows says of the guest's adapter - the host's own
 * answers would name a bus the guest has no part of - and AMD's free memory
 * from Windows' own account of the adapter's (ClIcdRoot::free_memory), which
 * Resolve asks. The vendor rows are reported on that vendor's adapter alone;
 * their other queries go to the host, which answers them where its own driver
 * carries the extension and refuses them where it does not.
 */
const ClExtensionPolicy::Row reported_extensions[] = {
    {"cl_khr_external_memory", 1u,
     "cl_buffer_paths.the_acquire_and_release_of_an_external_object_are_answered_by_this_driver",
     "cl_khr_gl_sharing"},
    {"cl_khr_external_memory_win32", 1u,
     "cl_buffer_paths.a_buffer_over_a_shared_object_is_opened_and_released_as_one",
     "cl_khr_gl_sharing"},
    {"cl_khr_gl_sharing", 1u, "cl_gl_sharing.a_context_made_from_the_current_gl_context_names_its_objects"},
    {"cl_khr_byte_addressable_store", 1u,
     "fullstack_opencl_icd.a_kernel_stores_single_bytes_and_leaves_their_neighbours"},
    {"cl_khr_d3d10_sharing", 1u, "cl_d3d10_sharing.a_dxgi_adapter_or_d3d10_device_on_the_avgpu_adapter_is_this_device",
     "-"},
    {"cl_khr_d3d11_sharing", 1u, "cl_d3d11_sharing.an_acquire_copies_each_resource_in_and_a_release_copies_it_back",
     "-"},
    {"cl_khr_pci_bus_info", 1u, "cl_device_report.the_pci_location_and_name_are_the_guest_adapter_s", "-*"},
    {"cl_amd_device_attribute_query", 1u,
     "cl_device_report.an_amd_adapter_says_where_it_sits_as_amd_s_driver_does", "-1002"},
    {"cl_nv_device_attribute_query", 1u,
     "cl_device_report.an_nvidia_adapter_says_where_it_sits_as_nvidia_s_driver_does", "-10DE"},
};
const uint32_t reported_extension_count = sizeof reported_extensions / sizeof reported_extensions[0];

ClExtensionPolicy::ClExtensionPolicy(const Row *rows, uint32_t count)
    : rows_(rows), count_(rows != nullptr ? count : 0u), vendor_(0u)
{
    AV_SCOPE("ClExtensionPolicy::ClExtensionPolicy");
    if (count_ > max_rows) count_ = max_rows;
}

bool ClExtensionPolicy::offered_by(const char *host, size_t host_bytes, const char *name) const
{
    AV_SCOPE("ClExtensionPolicy::offered_by");
    if (host == nullptr || name == nullptr) return false;
    const size_t bytes = length_within(host, host_bytes);
    size_t at = 0u;
    while (at < bytes) {
        while (at < bytes && host[at] == ' ') ++at;
        size_t end = at;
        while (end < bytes && host[end] != ' ') ++end;
        if (end > at && same_word(&host[at], name, end - at)) return true;
        at = end;
    }
    return false;
}

bool ClExtensionPolicy::answered_by(const char *host, size_t host_bytes, const Row &row) const
{
    AV_SCOPE("ClExtensionPolicy::answered_by");
    /* The host extension the row is answered over, when it is not the host's
     * own spelling of it: cl_khr_external_memory_win32 is answered over the
     * host's cl_khr_gl_sharing, since no Linux OpenCL has a Win32 handle to
     * take and the device opens the object through OpenGL instead
     * (research.md §21). */
    if (row.host_ != nullptr && row.host_[0] == answered_here[0]) {
        if (row.host_[1] == '\0') return true;
        /* "-*": answered here once the adapter is described, whatever it is. */
        if (row.host_[1] == '*' && row.host_[2] == '\0') return vendor_ != 0u;
        /* "-1002": answered here on an adapter of that PCI vendor. */
        uint32_t vendor = 0u;
        for (const char *digit = &row.host_[1]; *digit != '\0'; ++digit) {
            const char c = *digit;
            const uint32_t value = c >= '0' && c <= '9'   ? static_cast<uint32_t>(c - '0')
                                   : c >= 'A' && c <= 'F' ? static_cast<uint32_t>(c - 'A' + 10)
                                                          : 16u;
            if (value == 16u) return false;
            vendor = (vendor << 4u) | value;
        }
        return vendor_ != 0u && vendor == vendor_;
    }
    return offered_by(host, host_bytes, row.host_ != nullptr ? row.host_ : row.name_);
}

bool ClExtensionPolicy::reportable(const char *name) const
{
    AV_SCOPE("ClExtensionPolicy::reportable");
    if (name == nullptr) return false;
    for (uint32_t index = 0u; index < count_; ++index) {
        const char *const row = rows_[index].name_;
        size_t length = 0u;
        while (row[length] != '\0' && length < name_bytes) ++length;
        if (same_word(row, name, length)) return true;
    }
    return false;
}

size_t ClExtensionPolicy::report(const char *host, size_t host_bytes, char *into,
                                 size_t into_bytes) const
{
    AV_SCOPE("ClExtensionPolicy::report");
    /* Measured first, then written: a caller asks the size and asks again
     * with room, and an answer that does not fit writes nothing rather than
     * as much as happened to fit. */
    size_t needed = 1u;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (!answered_by(host, host_bytes, rows_[index])) continue;
        size_t length = 0u;
        while (rows_[index].name_[length] != '\0' && length < name_bytes) ++length;
        needed += length + (needed > 1u ? 1u : 0u);
    }
    if (into == nullptr || into_bytes < needed) return needed;

    size_t at = 0u;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (!answered_by(host, host_bytes, rows_[index])) continue;
        if (at != 0u) into[at++] = ' ';
        for (size_t letter = 0u; rows_[index].name_[letter] != '\0' && letter < name_bytes; ++letter) {
            into[at++] = rows_[index].name_[letter];
        }
    }
    into[at] = '\0';
    return needed;
}

void ClExtensionPolicy::observe_vendor(uint32_t vendor)
{
    AV_SCOPE("ClExtensionPolicy::observe_vendor");
    vendor_ = vendor;
}

uint32_t ClExtensionPolicy::rows() const
{
    AV_SCOPE("ClExtensionPolicy::rows");
    return count_;
}

bool ClExtensionPolicy::invariants_hold() const
{
    AV_SCOPE("ClExtensionPolicy::invariants_hold");
    if (count_ > max_rows) return false;
    if (count_ != 0u && rows_ == nullptr) return false;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (rows_[index].name_ == nullptr || rows_[index].test_ == nullptr) return false;
        size_t length = 0u;
        while (length < name_bytes && rows_[index].name_[length] != '\0') ++length;
        if (length == 0u || length == name_bytes) return false;
        /* No name twice: reporting one twice would be a list no application
         * can parse into a set. */
        for (uint32_t other = index + 1u; other < count_; ++other) {
            if (same_word(rows_[index].name_, rows_[other].name_, length)) return false;
        }
    }
    return true;
}

}  // namespace av::clicd
