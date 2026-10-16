/* One extension a driver may report, an entry of the constant table each
 * Khronos ICD keeps (Vulkan's ExtensionPolicy, OpenCL's ClExtensionPolicy,
 * OpenGL's GlExtensionPolicy), which tools/check_extension_policy.py holds to
 * a test per row (constitution VI). Its fields keep a member's trailing
 * underscore, as every row of this codebase does. */
#ifndef AV_ICD_EXTENSION_ROW_HPP
#define AV_ICD_EXTENSION_ROW_HPP

#include <stdint.h>

namespace av::icd {

struct ExtensionRow {
    const char *name_;
    uint32_t    spec_version_;
    /* The test that exercises the extension through the ICD, as suite.case. */
    const char *test_;
    /* The host extension this one is answered over, when it is not the host's
     * own spelling of it: VK_KHR_external_memory_win32 is answered over the
     * host's VK_KHR_external_memory_fd, since no Linux driver has a Win32
     * handle to give (feature 047, research.md §10). Null - the usual case -
     * means the host must have this extension itself, and is what a row that
     * says nothing about it gets. Each policy may give it a spelling of its
     * own (ClExtensionPolicy::answered_here). */
    const char *host_ = nullptr;
};

}  // namespace av::icd

#endif  // AV_ICD_EXTENSION_ROW_HPP
