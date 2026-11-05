#ifndef AV_COMMAND_HEADER_HPP
#define AV_COMMAND_HEADER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CommandHeader {
public:
    static constexpr size_t   encoded_size   = 8u;
    static constexpr uint32_t max_size       = 0x00FFFFFFu;
    static constexpr uint8_t  generate_reply = 1u;
    static constexpr uint16_t transport_api  = 0u;
    static constexpr uint32_t nop_id          = 0x00000001u;
    static constexpr uint32_t import_blob_id  = 0x00000002u;
    static constexpr uint32_t release_blob_id = 0x00000003u;
    static constexpr uint32_t share_object_id          = 0x00000004u;
    static constexpr uint32_t open_shared_object_id    = 0x00000005u;
    static constexpr uint32_t release_shared_object_id = 0x00000006u;
    static constexpr uint32_t lose_host_device_id      = 0x00000007u;
    static constexpr uint32_t host_device_lost         = 0u;
    static constexpr uint32_t host_device_kept         = 1u;
    static constexpr size_t   lose_host_device_answer  = 8u;
    static constexpr uint32_t piece_id                 = 0x00000008u;
    static constexpr uint32_t answer_piece_id          = 0x00000009u;
    static constexpr uint32_t channel_stats_id         = 0x0000000Au;

    CommandHeader();
    CommandHeader(uint32_t id, uint32_t size, uint8_t flags);

    uint32_t id() const;
    uint16_t api() const;
    uint32_t size() const;
    bool     generates_reply() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t flags_shift = 24u;

    uint32_t id_;
    uint32_t size_;
    uint8_t  flags_;
};

}

#endif
