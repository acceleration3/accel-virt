/* The context-addressed virtio-gpu requests (virtio 1.2 5.7.6.8): CTX_DESTROY
 * (the header's context id; 24 bytes), CTX_CREATE (name length and
 * context_init; 96 bytes with the debug name) and SUBMIT_3D (the size of the
 * command buffer that follows; 32 bytes). */
#ifndef AV_CONTEXT_REQUEST_HPP
#define AV_CONTEXT_REQUEST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class ContextRequest {
public:
    static constexpr size_t   header_size = 24u;
    static constexpr size_t   create_size = 96u;
    static constexpr size_t   submit_size = 32u;
    static constexpr uint32_t name_capacity = 64u;

    ContextRequest();

    /* From the start of the request; each is malformed when shorter than its size. */
    [[nodiscard]] Status decode_destroy(WireReader &reader);
    [[nodiscard]] Status decode_create(WireReader &reader);
    [[nodiscard]] Status decode_submit(WireReader &reader);
    uint32_t             context_id() const;
    uint32_t             name_length() const;
    /* The capset id in the low 8 bits; the rest reserved. */
    uint32_t             context_init() const;
    uint32_t             command_size() const;

private:
    static constexpr size_t context_id_offset = 16u;

    void read_header(WireReader &reader);

    uint32_t context_id_;
    uint32_t name_length_;
    uint32_t context_init_;
    uint32_t command_size_;
};

}  // namespace av

#endif  // AV_CONTEXT_REQUEST_HPP
