/* What one OpenGL context's calls leave in the guest that sizes or shapes the
 * next one (data-model.md): the pack and unpack pixel stores and the buffers
 * bound where a transfer reads or writes, so a transfer knows how many bytes
 * the application's pointer holds and whether it is an offset instead; the
 * element buffer of each vertex array, so a draw knows whether its indices are
 * an offset; each attribute's pointer, for the query that answers it; the
 * redundant texture, unit and sampler binds a draw loop repeats; the buffer
 * at every other target, so a command that writes through one knows whose
 * bytes change (BufferShadows); and the pointers a multi-draw names, as the
 * offsets the wire carries. */
#ifndef AV_GLICD_CLIENT_ARRAY_STATE_HPP
#define AV_GLICD_CLIENT_ARRAY_STATE_HPP

#include "icd/opengl/pixel_store.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::glicd {

struct ClientArrayStateProbe;

class ClientArrayState {
public:
    static constexpr uint32_t array_buffer = 0x8892u;
    static constexpr uint32_t element_array_buffer = 0x8893u;
    static constexpr uint32_t pixel_pack_buffer = 0x88EBu;
    static constexpr uint32_t pixel_unpack_buffer = 0x88ECu;
    static constexpr uint32_t query_buffer = 0x9192u;
    /* What bound_buffer answers where the binding is not known. */
    static constexpr uint32_t unknown = 0xFFFFFFFFu;
    /* The most pointers one multi-draw converts. */
    static constexpr size_t   max_offsets = 1024u;
    /* The vertex arrays whose element buffer is kept; past this many a draw
     * takes its indices as an offset, as the host then judges. */
    static constexpr uint32_t max_vertex_arrays = 64u;
    static constexpr uint32_t max_attributes = 16u;

    ClientArrayState();
    ClientArrayState(const ClientArrayState &) = delete;
    ClientArrayState &operator=(const ClientArrayState &) = delete;

    /* glPixelStore: false for a name it does not keep or a value the host
     * would refuse - either is still sent. */
    bool   store_pixel(uint32_t pname, int32_t value);
    /* What a glPixelStore name holds now; 0 for a name it does not keep. */
    int32_t pixel_value(uint32_t pname) const;
    size_t pixel_bytes(uint32_t format, uint32_t type, int32_t width, int32_t height, int32_t depth,
                       bool packing) const;
    /* A compressed upload's bytes: its image size; 0 for a negative one and
     * for a download, whose size the level answers. */
    size_t compressed_bytes(int32_t image_size, bool packing) const;

    void     bind_buffer(uint32_t target, uint32_t buffer);
    /* glBindBuffersBase and glBindBuffersRange bind by index; whether they
     * move an indexed target's own binding is not relied on, so it is no
     * longer known. Nothing for a target that is not indexed. */
    void     lose_buffer(uint32_t target);
    /* glPopClientAttrib put back array and pixel buffer bindings the guest
     * did not see: no buffer binding is known from now on. */
    void     restore_buffers();
    /* The buffer at `target` when the guest knows it for certain: false for
     * a target it does not keep, a binding lost or restored, and the element
     * array buffer, which a vertex array can change out of sight. */
    bool     known_buffer(uint32_t target, uint32_t &buffer) const;
    /* A deleted buffer is unbound from every target it was bound to. */
    void     delete_buffers(int32_t count, const uint32_t *buffers);
    uint32_t bound_buffer(uint32_t target) const;
    bool     pixel_buffer_bound(bool packing) const;
    bool     query_buffer_bound() const;
    void     bind_vertex_array(uint32_t array);
    /* A deleted vertex array's element buffer is forgotten; deleting the bound
     * one binds array 0, as OpenGL does. */
    void     delete_vertex_arrays(int32_t count, const uint32_t *arrays);
    /* Whether a draw's indices are an offset into an element buffer: true
     * when one is bound, or when the bound array's is not known. */
    bool     indices_are_offset() const;
    /* A vertex array at a client pointer, which a core context refuses: a
     * non-null pointer with no array buffer bound. */
    bool     client_pointer(uint64_t pointer) const;
    void     set_attribute_pointer(uint32_t index, uint64_t pointer);
    /* 0 for an attribute never pointed or past max_attributes. */
    uint64_t attribute_pointer(uint32_t index) const;

    /* `count` pointers as the 64-bit offsets the wire carries; null past
     * max_offsets or for a null list with a count. Valid until the next call. */
    const uint64_t *offsets_of(const void *const *pointers, size_t count);

    /* Whether this bind repeats what the context already has; each records the
     * bind it was asked about. */
    bool skip_active_texture(uint32_t texture);
    bool skip_bind_texture(uint32_t target, uint32_t texture);
    bool skip_bind_sampler(uint32_t unit, uint32_t sampler);
    /* No texture, unit or sampler bind is known any more: a call changed them
     * out of sight. */
    void forget_bindings();

    bool invariants_hold() const;

private:
    static constexpr uint32_t units = 32u;
    static constexpr uint32_t texture_targets = 11u;
    static constexpr uint32_t buffer_targets = 13u;
    static constexpr uint32_t texture0 = 0x84C0u;

    uint32_t    buffer_slot(uint32_t target) const;
    bool        indexed(uint32_t target) const;
    uint32_t    texture_slot(uint32_t target) const;
    const PixelStore *store_for(uint32_t pname, PixelStore::Field &field) const;
    /* The row of `array` in the vertex array table, or max_vertex_arrays. */
    uint32_t    vertex_array_row(uint32_t array) const;

    uint32_t   buffers_[buffer_targets];
    bool       buffers_restored_;
    PixelStore pack_;
    PixelStore unpack_;
    uint32_t   vertex_array_;
    uint32_t   array_names_[max_vertex_arrays];
    uint32_t   array_elements_[max_vertex_arrays];
    uint32_t   arrays_known_;
    uint64_t   attribute_pointers_[max_attributes];
    uint32_t   active_unit_;
    uint32_t   textures_[units][texture_targets];
    uint32_t   samplers_[units];
    uint64_t   offsets_[max_offsets];

    friend struct ClientArrayStateProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_CLIENT_ARRAY_STATE_HPP
