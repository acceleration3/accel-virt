/* What the generated OpenGL client reports when a command's encoding
 * overflowed and it was not sent (feature 050): the API, the command's name
 * and how far the writer got. Defined by the ICD that links the client
 * (src/icd/opengl/gl_hooks.cpp), one line into its log. */
#ifndef AV_PROTO_ICD_STUB_REPORT_HPP
#define AV_PROTO_ICD_STUB_REPORT_HPP

#include <stddef.h>

namespace av::icd {

void payload_dropped(const char *api, const char *command, size_t written) noexcept;

}  // namespace av::icd

#endif  // AV_PROTO_ICD_STUB_REPORT_HPP
