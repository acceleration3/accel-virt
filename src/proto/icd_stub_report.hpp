/* What a generated client and a generated ICD table report to the driver that
 * links them. Both are defined by that ICD (src/icd/opengl/gl_hooks.cpp,
 * src/icd/opencl/cl_hooks.cpp), one line into its log.
 *
 * payload_dropped: the OpenGL client's command whose encoding overflowed, so it
 * was not sent (feature 050) - the API, the command's name, how far the writer
 * got.
 *
 * stub_called: an ICD dispatch slot nothing else filled, refusing the call
 * (feature 052) - the API, the entry point's name, and why. Every slot of an
 * OpenCL cl_icd_dispatch must be filled, because the Khronos loader does not
 * check before it calls through, so the refusals are what stands in the slots
 * this driver does not serve. */
#ifndef AV_PROTO_ICD_STUB_REPORT_HPP
#define AV_PROTO_ICD_STUB_REPORT_HPP

#include <stddef.h>

namespace av::icd {

void payload_dropped(const char *api, const char *command, size_t written) noexcept;
void stub_called(const char *api, const char *entry_point, const char *why) noexcept;

}  // namespace av::icd

#endif  // AV_PROTO_ICD_STUB_REPORT_HPP
