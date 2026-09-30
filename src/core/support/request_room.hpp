/* Room for a request listing a frame's pages - a fixed part, then one entry a
 * page - whose size follows the display mode. It grows at PASSIVE, where
 * memory can be had, and is written at up to DISPATCH, where it cannot:
 * growing sets a spare aside that nothing writes, and the spare takes over
 * only when take() is asked for the pages, under whatever lock the writer
 * writes under. */
#ifndef AV_REQUEST_ROOM_HPP
#define AV_REQUEST_ROOM_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RequestRoom {
public:
    RequestRoom(Platform &platform, size_t fixed_bytes, size_t entry_bytes);
    ~RequestRoom();
    RequestRoom(const RequestRoom &) = delete;
    RequestRoom &operator=(const RequestRoom &) = delete;

    /* PASSIVE: a request listing `pages` fits once take() asks for it.
     * no_memory when the spare cannot be had; the room in use is untouched
     * either way. */
    [[nodiscard]] Status prepare(uint32_t pages);
    /* Up to DISPATCH, and never allocating: a request listing `pages` fits at
     * start() from now, the prepared spare taking over when the room in use is
     * smaller. no_memory when nothing prepared is big enough. */
    [[nodiscard]] Status take(uint32_t pages);
    uint8_t *start() const;
    size_t   size() const;

    bool invariants_hold() const;

private:
    size_t bytes_for(uint32_t pages) const;

    Platform &platform_;
    size_t    fixed_bytes_;
    size_t    entry_bytes_;
    uint8_t  *room_;
    size_t    room_bytes_;
    uint8_t  *spare_;
    size_t    spare_bytes_;
};

}  // namespace av

#endif  // AV_REQUEST_ROOM_HPP
