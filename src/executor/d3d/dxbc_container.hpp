#ifndef AV_DXBC_CONTAINER_HPP
#define AV_DXBC_CONTAINER_HPP

#include "proto/generated/d3d/d3d_wire_types.h"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DxbcContainer {
public:
    enum class Role : uint8_t { input, output, patch_constant };

    DxbcContainer(uint8_t *buffer, size_t capacity);

    [[nodiscard]] bool add_signature(Role role, const d3dproto::SignatureEntry *entries, uint32_t count,
                                     bool pixel_output);
    [[nodiscard]] bool add_layout_signature(const uint32_t *registers, uint32_t count);
    [[nodiscard]] bool add_code(const uint8_t *tokens, size_t size);
    [[nodiscard]] bool add_dxil(const uint8_t *program, size_t size);
    size_t             finish();

    const char *semantic_of(const d3dproto::SignatureEntry &entry, bool pixel_output, uint32_t &index) const;
    const char *stream_output_name(const d3dproto::SignatureEntry &entry, uint32_t &index) const;

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

}

#endif
