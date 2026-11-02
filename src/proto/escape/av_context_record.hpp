#ifndef AV_AV_CONTEXT_RECORD_HPP
#define AV_AV_CONTEXT_RECORD_HPP

#include "core/support/status.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class AvContextRecord {
public:
    static constexpr size_t   encoded_size    = 32u;
    static constexpr uint32_t max_reply_bytes = 16u << 20u;

    AvContextRecord();
    AvContextRecord(SubApi sub_api, uint8_t ring_order, uint32_t ring, uint32_t reply_bytes,
                     const GlContextOptions &options = GlContextOptions());

    SubApi   sub_api() const;
    uint8_t  ring_order() const;
    uint32_t ring() const;
    uint32_t reply_bytes() const;
    GlContextOptions options() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic         = 0x004B5641u;
    static constexpr uint8_t  lowest_order  = 12u;
    static constexpr uint8_t  highest_order = 24u;

    bool fields_valid() const;

    uint32_t sub_api_;
    uint8_t  ring_order_;
    uint32_t ring_;
    uint32_t reply_bytes_;
    GlContextOptions options_;
};

}

#endif
