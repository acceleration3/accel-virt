#ifndef AV_PATCH_REQUEST_HPP
#define AV_PATCH_REQUEST_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct DxgkargPatchRecord;

class PatchRequest {
public:
    static constexpr uint32_t address_bytes = 8u;

    PatchRequest(Platform &platform, const DxgkargPatchRecord &request);
    PatchRequest(const PatchRequest &) = delete;
    PatchRequest &operator=(const PatchRequest &) = delete;

    [[nodiscard]] Status patch();

private:
    [[nodiscard]] Status validate() const;

    Platform                 &platform_;
    const DxgkargPatchRecord &request_;
};

}

#endif
