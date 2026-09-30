/* What the present engine's page says (docs/protocol.md, "Present engine"):
 * the last ticket the device finished with, and how many of those it refused,
 * under the page's magic and version. The device writes the whole record when
 * the page is registered and each count after as one aligned store at its
 * offset; the guest decodes what it read as the hostile bytes they are. */
#ifndef AV_PRESENT_PROGRESS_HPP
#define AV_PRESENT_PROGRESS_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PresentProgress {
public:
    static constexpr size_t page_size = 4096u;
    static constexpr size_t encoded_size = 24u;
    static constexpr size_t completed_at = 8u;
    static constexpr size_t refused_at = 16u;

    PresentProgress();
    PresentProgress(uint64_t completed, uint64_t refused);

    uint64_t completed() const;
    uint64_t refused() const;

    /* too_large for less room than the record. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for a wrong magic or version, or more refused than completed.
     * Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x504B5652u;   /* "RVKP" */
    static constexpr uint32_t version = 1u;

    uint64_t completed_;
    uint64_t refused_;
};

}  // namespace av

#endif  // AV_PRESENT_PROGRESS_HPP
