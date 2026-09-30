/* The private data a process puts on D3DKMTCreateContextVirtual to ask the
 * miniport for an rvk context rather than a plain scheduling one.
 *
 * It names the sub-API, the ring order, the size of the reply slot, and the
 * process's allocation that stands for the context: the one handle a driver
 * can resolve in the process that made it, which is how another context of
 * that process names this one (feature 050). The memory the context runs on -
 * ring, fence page, reply slot - is the driver's own, handed to the process
 * after the context exists (docs/protocol.md, "Context memory"). The kernel
 * side decodes it from memory a process wrote, so it is hostile input and
 * every field is judged, the way ContextSetup judges the wire. It carries no
 * doorbell: the miniport owns and rings the doorbell (feature 043). Its last
 * eight bytes are an OpenGL context's options, zero for any other sub-API. */
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
    static constexpr size_t   encoded_size    = 32u;
    /* The most a reply slot may be: the largest one a driver asks for is a
     * megabyte, and the slot is pages the driver holds for the context's life. */
    static constexpr uint32_t max_reply_bytes = 16u << 20u;

    RvkContextRecord();
    RvkContextRecord(SubApi sub_api, uint8_t ring_order, uint32_t ring, uint32_t reply_bytes,
                     const GlContextOptions &options = GlContextOptions());

    SubApi   sub_api() const;
    uint8_t  ring_order() const;
    /* The allocation that names the context. */
    uint32_t ring() const;
    uint32_t reply_bytes() const;
    GlContextOptions options() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Checks what the bytes alone can prove; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    /* "RVKK": a record of the kind before the driver owned the memory named
     * three allocations under another magic, and is refused whole. */
    static constexpr uint32_t magic         = 0x4B4B5652u;
    static constexpr uint8_t  lowest_order  = 12u;
    static constexpr uint8_t  highest_order = 24u;

    bool fields_valid() const;

    uint32_t sub_api_;
    uint8_t  ring_order_;
    uint32_t ring_;
    uint32_t reply_bytes_;
    GlContextOptions options_;
};

}  // namespace av

#endif  // AV_RVK_CONTEXT_RECORD_HPP
