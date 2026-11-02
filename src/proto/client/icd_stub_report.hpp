#ifndef AV_PROTO_ICD_STUB_REPORT_HPP
#define AV_PROTO_ICD_STUB_REPORT_HPP

#include <stddef.h>

namespace av::icd {

void payload_dropped(const char *api, const char *command, size_t written) noexcept;
void stub_called(const char *api, const char *entry_point, const char *why) noexcept;

}

#endif
