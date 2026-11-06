#ifndef AV_REQUEST_ROOM_HPP
#define AV_REQUEST_ROOM_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct RequestRoomProbe;

class RequestRoom {
public:
    RequestRoom(Platform &platform, size_t fixed_bytes, size_t entry_bytes);
    ~RequestRoom();
    RequestRoom(const RequestRoom &) = delete;
    RequestRoom &operator=(const RequestRoom &) = delete;

    [[nodiscard]] Status prepare(uint32_t pages);
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

    friend struct RequestRoomProbe;
};

}

#endif
