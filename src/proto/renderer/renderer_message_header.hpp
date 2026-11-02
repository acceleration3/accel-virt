#ifndef AV_RENDERER_MESSAGE_HEADER_HPP
#define AV_RENDERER_MESSAGE_HEADER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RendererMessageHeader {
public:
    enum class Kind : uint32_t {
        hello = 1u,
        ready,
        region,
        guest_blob,
        allocate_memory,
        blob_retired,
        create_context,
        bind_context,
        destroy_context,
        interrupt_lines,
        interrupts_off,
        reply,
        export_image,
        exported_image,
        shared_released,
        allocated_memory,
        free_memory
    };

    static constexpr size_t   encoded_size = 16u;
    static constexpr uint32_t max_body_length = 0x40010u;

    RendererMessageHeader();
    RendererMessageHeader(Kind kind, uint32_t body_length, uint64_t sequence);

    Kind     kind() const;
    uint32_t body_length() const;
    uint64_t sequence() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool fields_valid() const;

    uint32_t kind_;
    uint32_t body_length_;
    uint64_t sequence_;
};

}

#endif
