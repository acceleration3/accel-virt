/* QEMU -> renderer: the guest let go of a shared object everywhere (the
 * driver's SharedReleaseRequest), or, for object 0, the device was reset and
 * the guest's ids start again. The renderer's contexts hold what was shared;
 * this is what lets it go. */
#ifndef AV_SHARED_RELEASED_MESSAGE_HPP
#define AV_SHARED_RELEASED_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedReleasedMessage {
public:
    static constexpr size_t   encoded_size = 8u;
    static constexpr uint32_t descriptors = 0u;

    SharedReleasedMessage();
    explicit SharedReleasedMessage(uint64_t object_id);

    /* 0 for every object. */
    uint64_t object_id() const;

    /* too_large for less room than encoded_size. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length or any descriptor. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint64_t object_id_;
};

}  // namespace av

#endif  // AV_SHARED_RELEASED_MESSAGE_HPP
