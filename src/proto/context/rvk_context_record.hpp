/* The private data a process puts on D3DKMTCreateContextVirtual to ask the
 * miniport for an rvk context rather than a plain scheduling one.
 *
 * It names the sub-API, the ring order, and which of the context's own
 * allocations are the ring, the fence page and (optionally) the reply. The
 * kernel side decodes it from memory a process wrote, so it is hostile input
 * and every field is judged, the way ContextSetup judges the wire. It carries
 * no doorbell: the miniport owns and rings the doorbell (feature 043). Its last
 * eight bytes are an OpenGL context's options, zero for any other sub-API
 * (feature 050). */
#ifndef AV_RVK_CONTEXT_RECORD_HPP
#define AV_RVK_CONTEXT_RECORD_HPP

#include "core/support/status.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RvkContextRecord {
public:
    static constexpr size_t encoded_size = 32u;

    RvkContextRecord();
    RvkContextRecord(SubApi sub_api, uint8_t ring_order, uint32_t ring, uint32_t fence, uint32_t reply,
                     const GlContextOptions &options = GlContextOptions());

    SubApi   sub_api() const;
    uint8_t  ring_order() const;
    uint32_t ring() const;
    uint32_t fence() const;
    /* 0 when the context reads no answer back. */
    uint32_t reply() const;
    GlContextOptions options() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Checks what the bytes alone can prove; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic         = 0x544B5652u;  /* "RVKT" */
    static constexpr uint8_t  lowest_order  = 12u;
    static constexpr uint8_t  highest_order = 24u;

    bool fields_valid() const;

    uint32_t sub_api_;
    uint8_t  ring_order_;
    uint32_t ring_;
    uint32_t fence_;
    uint32_t reply_;
    GlContextOptions options_;
};

}  // namespace av

#endif  // AV_RVK_CONTEXT_RECORD_HPP
