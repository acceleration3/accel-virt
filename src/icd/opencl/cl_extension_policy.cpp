#include "icd/opencl/cl_extension_policy.hpp"

#include "core/support/extension_words.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/generated/opencl/cl_supported_extensions.h"

namespace av::clicd {

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
 * acquired for its life. Both are answered over the device's
 * `cl_khr_external_memory`, which is the executor's to say and not the host
 * OpenCL's: no OpenCL on a Linux host has a Win32 handle to take, and the
 * device reaches the descriptor through OpenGL instead (research.md §18 and
 * §21), which needs the host's OpenCL to share with the executor's GL. The
 * executor says the name exactly where that import is made
 * (OpenClBackend::on_clGetDeviceInfo) - not on NVIDIA's, which shares with no
 * EGL context - so neither row is reported where it would fail.
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
 * the executor leaves it in exactly where a GL context can be had on the GPU.
 * The context this ICD makes from a GL context is a context of the device's
 * made in that GL context's share group: the OpenGL ICD names the context by
 * its ring (avGetShareName), the miniport resolves the name for a context of
 * the same process, and the executor reaches the guest one's objects through a
 * GL context sharing them - the host's own OpenCL naming them where it shares
 * with that context, and copies at the acquire and the release where it does
 * not (CopiedGlSharing).
 *
 * `cl_khr_pci_bus_info`, `cl_amd_device_attribute_query` and
 * `cl_nv_device_attribute_query` say where the device sits on the PCI bus,
 * which is how an application pairs it with a DXGI adapter or a vendor
 * library's GPU (docs/vendor-libraries.md; DaVinci Resolve does). ClDeviceReport
 * answers them from what Windows says of the guest's adapter - the host's own
 * answers would name a bus the guest has no part of - and AMD's free memory
 * from Windows' own account of the adapter's (ClIcdRoot::free_memory), which
 * Resolve asks. The vendor rows are reported on that vendor's adapter alone;
 * an Intel adapter has none, and Resolve pairs it with its DXGI adapter by
 * `cl_khr_device_uuid`'s LUID instead - answered here from the guest's
 * adapter, its UUIDs by the host - and found no OpenCL GPU while the
 * extension went unreported.
 * their other queries go to the host, which answers them where its own driver
 * carries the extension and refuses them where it does not.
 *
 * The kernel-language and image extensions after them are the host
 * compiler's and the host image's: a program is compiled by the host, so what
 * its compiler accepts is what a kernel here may use, and the images are the
 * host's own objects. Each is reported where the host has it and proven by a
 * kernel run through this ICD.
 */
const ClExtensionPolicy::Row reported_extensions[] = {
    {"cl_khr_external_memory", 1u,
     "cl_buffer_paths.the_acquire_and_release_of_an_external_object_are_answered_by_this_driver"},
    {"cl_khr_external_memory_win32", 1u,
     "cl_buffer_paths.a_buffer_over_a_shared_object_is_opened_and_released_as_one",
     "cl_khr_external_memory"},
    {"cl_khr_gl_sharing", 1u, "cl_gl_sharing.a_context_made_from_the_current_gl_context_names_its_objects"},
    {"cl_khr_byte_addressable_store", 1u,
     "fullstack_opencl_icd.a_kernel_stores_single_bytes_and_leaves_their_neighbours"},
    {"cl_khr_d3d10_sharing", 1u, "cl_d3d10_sharing.a_dxgi_adapter_or_d3d10_device_on_the_avgpu_adapter_is_this_device",
     "-"},
    {"cl_khr_d3d11_sharing", 1u, "cl_d3d11_sharing.an_acquire_copies_each_resource_in_and_a_release_copies_it_back",
     "-"},
    {"cl_khr_pci_bus_info", 1u, "cl_device_report.the_pci_location_and_name_are_the_guest_adapter_s", "-*"},
    {"cl_khr_device_uuid", 1u, "cl_device_report.the_luid_is_the_adapters_own_bytes_the_way_windows_reads_them"},
    {"cl_amd_device_attribute_query", 1u,
     "cl_device_report.an_amd_adapter_says_where_it_sits_as_amd_s_driver_does", "-1002"},
    {"cl_nv_device_attribute_query", 1u,
     "cl_device_report.an_nvidia_adapter_says_where_it_sits_as_nvidia_s_driver_does", "-10DE"},
    {"cl_khr_fp16", 1u, "fullstack_opencl_icd.a_kernel_computes_in_half_precision_where_the_host_has_it"},
    {"cl_khr_fp64", 1u, "fullstack_opencl_icd.a_kernel_computes_in_double_precision_where_the_host_has_it"},
    {"cl_khr_global_int32_base_atomics", 1u,
     "fullstack_opencl_icd.a_kernel_s_32_bit_atomics_on_global_and_local_memory_all_land"},
    {"cl_khr_global_int32_extended_atomics", 1u,
     "fullstack_opencl_icd.a_kernel_s_32_bit_atomics_on_global_and_local_memory_all_land"},
    {"cl_khr_local_int32_base_atomics", 1u,
     "fullstack_opencl_icd.a_kernel_s_32_bit_atomics_on_global_and_local_memory_all_land"},
    {"cl_khr_local_int32_extended_atomics", 1u,
     "fullstack_opencl_icd.a_kernel_s_32_bit_atomics_on_global_and_local_memory_all_land"},
    {"cl_khr_int64_base_atomics", 1u, "fullstack_opencl_icd.a_kernel_s_64_bit_atomics_all_land_where_the_host_has_them"},
    {"cl_khr_int64_extended_atomics", 1u,
     "fullstack_opencl_icd.a_kernel_s_64_bit_atomics_all_land_where_the_host_has_them"},
    {"cl_khr_3d_image_writes", 1u, "fullstack_opencl_icd.a_kernel_writes_a_3d_image_and_its_texels_come_back"},
    {"cl_khr_image2d_from_buffer", 1u, "fullstack_opencl_icd.a_2d_image_over_a_buffer_reads_the_buffer_s_bytes"},
    {"cl_khr_depth_images", 1u, "fullstack_opencl_icd.a_depth_image_a_kernel_writes_reads_back"},
    {"cl_khr_il_program", 1u, "fullstack_opencl_icd.a_program_from_spir_v_runs_its_kernels_and_says_what_it_was_made_of"},
};
const uint32_t reported_extension_count = sizeof reported_extensions / sizeof reported_extensions[0];

ClExtensionPolicy::ClExtensionPolicy(const Row *rows, uint32_t count)
    : rows_(rows, count < max_rows ? count : max_rows, "cl_",
            icd::ExtensionRows::Names(clproto::kCarriedExtensions,
                                      sizeof clproto::kCarriedExtensions / sizeof clproto::kCarriedExtensions[0]),
            icd::ExtensionRows::Names(nullptr, 0u)),
      vendor_(0u)
{
    AV_SCOPE("ClExtensionPolicy::ClExtensionPolicy");
}

uint32_t ClExtensionPolicy::vendor_of(const char *host) const
{
    AV_SCOPE("ClExtensionPolicy::vendor_of");
    uint32_t vendor = 0u;
    for (const char *digit = &host[1]; *digit != '\0'; ++digit) {
        const char     c = *digit;
        const uint32_t value = c >= '0' && c <= '9'   ? static_cast<uint32_t>(c - '0')
                               : c >= 'A' && c <= 'F' ? static_cast<uint32_t>(c - 'A' + 10)
                                                      : 16u;
        if (value == 16u) return 0u;
        vendor = (vendor << 4u) | value;
    }
    return vendor;
}

bool ClExtensionPolicy::answered_by(const ExtensionWords &host, uint32_t index) const
{
    AV_SCOPE("ClExtensionPolicy::answered_by");
    if (!rows_.reportable(index)) return false;
    /* The host extension the row is answered over, when it is not the host's
     * own spelling of it: cl_khr_external_memory_win32 is answered over the
     * device's cl_khr_external_memory, since no Linux OpenCL has a Win32
     * handle to take and the device opens the object through OpenGL instead
     * (research.md §21). */
    const char *const over = rows_.row(index).host_;
    if (over != nullptr && over[0] == answered_here[0]) {
        if (over[1] == '\0') return true;
        /* "-*": answered here once the adapter is described, whatever it is. */
        if (over[1] == '*' && over[2] == '\0') return vendor_ != 0u;
        /* "-1002": answered here on an adapter of that PCI vendor. */
        return vendor_ != 0u && vendor_of(over) == vendor_;
    }
    return host.holds(rows_.host_name(index));
}

bool ClExtensionPolicy::reportable(const char *name) const
{
    AV_SCOPE("ClExtensionPolicy::reportable");
    return rows_.named(name) != rows_.count();
}

bool ClExtensionPolicy::reports(const ExtensionWords &host, const char *name) const
{
    AV_SCOPE("ClExtensionPolicy::reports");
    const uint32_t index = rows_.named(name);
    return index != rows_.count() && answered_by(host, index);
}

size_t ClExtensionPolicy::report(const ExtensionWords &host, char *into, size_t into_bytes) const
{
    AV_SCOPE("ClExtensionPolicy::report");
    /* Measured first, then written: a caller asks the size and asks again
     * with room, and an answer that does not fit writes nothing rather than
     * as much as happened to fit. */
    size_t needed = 1u;
    for (uint32_t index = 0u; index < rows_.count(); ++index) {
        if (!answered_by(host, index)) continue;
        needed += __builtin_strlen(rows_.row(index).name_) + (needed > 1u ? 1u : 0u);
    }
    if (into == nullptr || into_bytes < needed) return needed;

    size_t at = 0u;
    for (uint32_t index = 0u; index < rows_.count(); ++index) {
        if (!answered_by(host, index)) continue;
        if (at != 0u) into[at++] = ' ';
        const char *const name = rows_.row(index).name_;
        for (size_t letter = 0u; name[letter] != '\0'; ++letter) into[at++] = name[letter];
    }
    into[at] = '\0';
    return needed;
}

size_t ClExtensionPolicy::report_versions(const ExtensionWords &host, cl_name_version *into, size_t room) const
{
    AV_SCOPE("ClExtensionPolicy::report_versions");
    size_t needed = 0u;
    for (uint32_t index = 0u; index < rows_.count(); ++index) {
        if (answered_by(host, index)) ++needed;
    }
    if (into == nullptr || room < needed) return needed;
    size_t at = 0u;
    for (uint32_t index = 0u; index < rows_.count(); ++index) {
        if (!answered_by(host, index)) continue;
        const Row       &row = rows_.row(index);
        cl_name_version &entry = into[at++];
        entry.version = CL_MAKE_VERSION(row.spec_version_, 0u, 0u);
        size_t letter = 0u;
        for (; row.name_[letter] != '\0' && letter + 1u < CL_NAME_VERSION_MAX_NAME_SIZE; ++letter)
            entry.name[letter] = row.name_[letter];
        for (; letter < CL_NAME_VERSION_MAX_NAME_SIZE; ++letter) entry.name[letter] = '\0';
    }
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
    return rows_.count();
}

bool ClExtensionPolicy::invariants_hold() const
{
    AV_SCOPE("ClExtensionPolicy::invariants_hold");
    return rows_.count() <= max_rows && rows_.invariants_hold();
}

}  // namespace av::clicd
