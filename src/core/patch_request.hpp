/* One DxgkDdiPatch call: the segment address of each allocation a submission
 * references is written into its DMA buffer at the recorded offsets. The whole
 * patch is validated before the first write, so a bad location leaves the
 * buffer as it was. Retired with physical addressing by feature 017. */
#ifndef AV_PATCH_REQUEST_HPP
#define AV_PATCH_REQUEST_HPP

#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

struct DxgkargPatchRecord;

class PatchRequest {
public:
    /* A patch writes the 64-bit address plus offset, little endian. */
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

}  // namespace av

#endif  // AV_PATCH_REQUEST_HPP
