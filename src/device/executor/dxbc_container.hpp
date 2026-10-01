/* A DXBC container, written from what the DDI hands a driver instead of one.
 *
 * The Direct3D runtime gives a user-mode driver a shader's token stream and
 * its signatures as registers, masks and system values - never the container
 * the application compiled, and never a semantic name. DXVK takes nothing but
 * a container. So the executor writes one: the tokens as a SHEX chunk, each
 * signature as an ISG1/OSG1/OSG5/PSG1 chunk, and the checksum over all of it.
 *
 * Names are invented where the DDI has none. A system value gets its SV_ name
 * and the semantic index Direct3D numbers it with - the tessellation factors
 * the DDI spells as one name per edge are one name here with the edge as the
 * index. Everything else is AV with the register and first component as its
 * index, which is unique within a signature and is what the input layouts the
 * executor writes name too (DxbcContainer::add_layout_signature): stages link
 * by register, not by name, so no two sides ever have to agree on more. */
#ifndef AV_DXBC_CONTAINER_HPP
#define AV_DXBC_CONTAINER_HPP

#include "proto/generated/d3d/d3d_wire_types.h"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DxbcContainer {
public:
    enum class Role : uint8_t { input, output, patch_constant };

    /* Writes into `capacity` bytes at `buffer`, which the caller keeps. */
    DxbcContainer(uint8_t *buffer, size_t capacity);

    /* A signature of `count` entries; `pixel_output` names the pixel stage's
     * outputs as render targets, `with_streams` writes OSG5 for a geometry
     * shader that streams out. False when it does not fit. */
    [[nodiscard]] bool add_signature(Role role, const d3dproto::SignatureEntry *entries, uint32_t count,
                                     bool pixel_output);
    /* An input signature naming every register an input layout feeds, one
     * whole register per element. */
    [[nodiscard]] bool add_layout_signature(const uint32_t *registers, uint32_t count);
    /* The token stream as the DDI gave it: its second token is its length in
     * tokens, and a stream whose length disagrees with `size` is refused. */
    [[nodiscard]] bool add_code(const uint8_t *tokens, size_t size);
    /* A chunk holding `size` bytes of `program` as they are: a DXIL program
     * (its header's third word is DXIL's magic) under "DXIL". False when the
     * bytes are not one or do not fit. */
    [[nodiscard]] bool add_dxil(const uint8_t *program, size_t size);
    /* The header and checksum over what was added; the container's size, or 0
     * when nothing fit. */
    size_t             finish();

    /* The semantic name and index the executor gives a DDI signature entry,
     * which the stream-output declaration has to name the same way. */
    const char *semantic_of(const d3dproto::SignatureEntry &entry, bool pixel_output, uint32_t &index) const;

private:
    static constexpr uint32_t max_chunks = 6u;
    static constexpr size_t   header_bytes = 32u + 4u * max_chunks;

    bool     reserve(size_t bytes);
    uint32_t read_u32(const uint8_t *at) const;
    void     put_u32(uint32_t value);
    void     put_bytes(const void *data, size_t size);
    void     patch_u32(size_t at, uint32_t value);
    bool     begin_chunk(const char *fourcc);
    void     end_chunk();

    uint8_t *buffer_;
    size_t   capacity_;
    size_t   used_;
    uint32_t chunks_;
    size_t   chunk_offsets_[max_chunks];
    bool     overflowed_;
};

}  // namespace av

#endif  // AV_DXBC_CONTAINER_HPP
