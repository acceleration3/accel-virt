#ifndef AV_GLICD_CLIENT_VERTEX_DRAW_HPP
#define AV_GLICD_CLIENT_VERTEX_DRAW_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {

class BufferShadows;
class ClientArrayState;
class ClientVertexState;

class ClientVertexDraw {
public:
    static constexpr uint64_t most_staged = uint64_t{128} << 20u;
    static constexpr uint32_t gl_invalid_enum = 0x0500u;
    static constexpr uint32_t gl_invalid_value = 0x0501u;
    static constexpr uint32_t gl_invalid_operation = 0x0502u;
    static constexpr uint32_t gl_out_of_memory = 0x0505u;

    ClientVertexDraw(ClientTransport &wire, ClientVertexState &state, ClientArrayState &buffers,
                     BufferShadows &shadows);
    ClientVertexDraw(const ClientVertexDraw &) = delete;
    ClientVertexDraw &operator=(const ClientVertexDraw &) = delete;

    void pointer(uint32_t slot, int32_t size, uint32_t type, bool normalized, int32_t stride, const void *pointer);
    void enable(uint32_t slot, bool enabled);
    uint32_t active_texture(uint32_t texture);
    uint32_t interleaved(uint32_t format, int32_t stride, const void *pointer);
    void begin_immediate();
    void array_element(int32_t index);

    uint32_t arrays(int64_t first, int64_t count);
    uint32_t elements(uint32_t type, int64_t count, const void *indices, int64_t base_vertex, uint64_t &host_indices);
    uint32_t multi_arrays(const int32_t *first, const int32_t *count, int32_t draws);
    uint32_t multi_elements(const int32_t *count, uint32_t type, const void *const *indices, int32_t draws,
                            const int32_t *base_vertex, const uint64_t *&host_indices);
    uint32_t indirect();
    void     finish();

    bool invariants_hold() const;

private:
    struct Range {
        int64_t  lo_;
        int64_t  hi_;
        bool     any_;
    };

    uint32_t user_array_buffer();
    uint32_t user_element_buffer();
    uint32_t stage(const Range &range, uint64_t index_bytes, uint64_t &index_at);
    void     put(uint64_t at, const uint8_t *from, uint64_t bytes);
    void     commit(bool with_indices);
    uint32_t scan(uint32_t type, int64_t count, const void *indices, int64_t base_vertex, Range &range);
    void     host_pointer(uint32_t slot, uint64_t offset);
    void     host_enable(uint32_t slot, bool enabled);
    void     host_unit(uint32_t unit);
    uint32_t element_bytes(uint32_t slot) const;
    uint32_t stride_of(uint32_t slot) const;
    bool     emit(uint32_t slot, int32_t index);
    void     fold(Range &range, int64_t lo, int64_t hi) const;

    ClientTransport   &wire_;
    ClientVertexState &state_;
    ClientArrayState  &buffers_;
    BufferShadows     &shadows_;
    uint32_t           user_array_;
    bool               staged_;
};

}

#endif
