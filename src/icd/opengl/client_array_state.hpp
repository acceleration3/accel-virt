#ifndef AV_GLICD_CLIENT_ARRAY_STATE_HPP
#define AV_GLICD_CLIENT_ARRAY_STATE_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "icd/opengl/pixel_store.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::glicd {

struct ClientArrayStateProbe;

class ClientArrayState {
public:
    static constexpr uint32_t array_buffer = 0x8892u;
    static constexpr uint32_t element_array_buffer = 0x8893u;
    static constexpr uint32_t pixel_pack_buffer = 0x88EBu;
    static constexpr uint32_t pixel_unpack_buffer = 0x88ECu;
    static constexpr uint32_t query_buffer = 0x9192u;
    static constexpr uint32_t transform_feedback_buffer = 0x8C8Eu;
    static constexpr uint32_t unknown = 0xFFFFFFFFu;
    static constexpr uint32_t max_attributes = 16u;
    static constexpr uint32_t client_pixel_store_bit = 0x00000001u;
    static constexpr uint32_t client_vertex_array_bit = 0x00000002u;
    static constexpr uint32_t most_pushed = 64u;
    static constexpr uint32_t least_depth_limit = 16u;
    static constexpr uint32_t max_depth_query = 0x0D3Bu;

    explicit ClientArrayState(BlockSource &memory);
    ClientArrayState(const ClientArrayState &) = delete;
    ClientArrayState &operator=(const ClientArrayState &) = delete;

    bool   store_pixel(uint32_t pname, int32_t value);
    int32_t pixel_value(uint32_t pname) const;
    size_t pixel_bytes(uint32_t format, uint32_t type, int32_t width, int32_t height, int32_t depth,
                       bool packing) const;
    size_t compressed_bytes(int32_t image_size, bool packing) const;
    size_t bitmap_bytes(int32_t width, int32_t height, bool packing) const;

    void     bind_buffer(uint32_t target, uint32_t buffer);
    bool     tracks(uint32_t target) const;
    void     push_client(uint32_t mask);
    void     pop_client();
    void     limit_client_depth(int32_t reported);
    void     forget_buffer(uint32_t target);
    void     reset_client(uint32_t mask);
    void     set_vertex_array_elements(uint32_t array, uint32_t buffer);
    void     delete_buffers(int32_t count, const uint32_t *buffers);
    uint32_t bound_buffer(uint32_t target) const;
    bool     pixel_buffer_bound(bool packing) const;
    bool     query_buffer_bound() const;
    [[nodiscard]] Status bind_vertex_array(uint32_t array);
    void     delete_vertex_arrays(int32_t count, const uint32_t *arrays);
    bool     indices_are_offset() const;
    bool     client_pointer(uint64_t pointer) const;
    void     set_attribute_pointer(uint32_t index, uint64_t pointer);
    uint64_t attribute_pointer(uint32_t index) const;

    const uint64_t *offsets_of(const void *const *pointers, size_t count);
    const int64_t  *widened(const intptr_t *values, size_t count, uint32_t which);

    bool skip_active_texture(uint32_t texture);
    bool skip_bind_texture(uint32_t target, uint32_t texture);
    bool skip_bind_sampler(uint32_t unit, uint32_t sampler);
    void forget_bindings();

    bool invariants_hold() const;

private:
    static constexpr uint32_t units = 32u;
    static constexpr uint32_t texture_targets = 11u;
    static constexpr uint32_t buffer_targets = 13u;
    static constexpr uint32_t texture0 = 0x84C0u;

    struct Pushed {
        uint32_t   mask_;
        uint32_t   array_buffer_;
        uint32_t   pack_buffer_;
        uint32_t   unpack_buffer_;
        uint32_t   vertex_array_;
        uint32_t   elements_;
        PixelStore pack_;
        PixelStore unpack_;
    };

    void        forget_unsaved_bindings();
    uint32_t    buffer_slot(uint32_t target) const;
    uint32_t    texture_slot(uint32_t target) const;
    const PixelStore *store_for(uint32_t pname, PixelStore::Field &field) const;
    uint32_t       *elements_of(uint32_t array);
    const uint32_t *elements_of(uint32_t array) const;

    uint32_t   buffers_[buffer_targets];
    Pushed     pushed_[most_pushed];
    uint32_t   depth_;
    uint32_t   unsaved_;
    uint32_t   host_depth_limit_;
    PixelStore pack_;
    PixelStore unpack_;
    uint32_t   vertex_array_;
    uint32_t                       default_elements_;
    KeyedSlots<uint32_t, uint32_t> elements_;
    uint64_t   attribute_pointers_[max_attributes];
    uint32_t   active_unit_;
    uint32_t   textures_[units][texture_targets];
    uint32_t   samplers_[units];
    GrowableArray<uint64_t> offsets_;
    GrowableArray<int64_t>  wide_[2];
    uint64_t                no_offsets_;

    friend struct ClientArrayStateProbe;
};

}

#endif
