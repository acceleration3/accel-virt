#ifndef AV_GLICD_CLIENT_VERTEX_STATE_HPP
#define AV_GLICD_CLIENT_VERTEX_STATE_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::glicd {

struct ClientVertexStateProbe;

class ClientVertexState {
public:
    static constexpr uint32_t generic_slots = 16u;
    static constexpr uint32_t vertex_slot = 16u;
    static constexpr uint32_t normal_slot = 17u;
    static constexpr uint32_t color_slot = 18u;
    static constexpr uint32_t secondary_color_slot = 19u;
    static constexpr uint32_t index_slot = 20u;
    static constexpr uint32_t edge_flag_slot = 21u;
    static constexpr uint32_t fog_coord_slot = 22u;
    static constexpr uint32_t texcoord_slot = 23u;
    static constexpr uint32_t texcoord_units = 32u;
    static constexpr uint32_t slot_count = texcoord_slot + texcoord_units;
    static constexpr uint32_t no_slot = 0xFFFFFFFFu;
    static constexpr uint32_t client_vertex_array_bit = 0x00000002u;
    static constexpr uint32_t most_pushed = 64u;
    static constexpr uint32_t texture0 = 0x84C0u;

    enum class Mode : uint8_t { unset, buffer, client };

    struct Slot {
        uint64_t pointer_;
        int32_t  size_;
        uint32_t type_;
        int32_t  stride_;
        uint8_t  mode_;
        uint8_t  enabled_;
        uint8_t  host_enabled_;
        uint8_t  normalized_;
    };

    struct Arrays {
        Slot     slots_[slot_count];
        uint64_t client_;
        uint64_t host_;
    };

    explicit ClientVertexState(BlockSource &memory);
    ClientVertexState(const ClientVertexState &) = delete;
    ClientVertexState &operator=(const ClientVertexState &) = delete;

    [[nodiscard]] Status bind(uint32_t array);
    void                 forget(int32_t count, const uint32_t *arrays);
    uint32_t             bound() const;
    const Arrays        &current() const;
    const Slot          &slot(uint32_t index) const;
    bool                 client_active() const;
    uint64_t             client_slots() const;
    uint64_t             host_slots() const;

    uint32_t slot_of_capability(uint32_t capability) const;
    uint32_t texcoord_unit() const;
    bool     select_texcoord_unit(uint32_t texture);
    uint32_t texcoord_slot_of_unit(uint32_t unit) const;
    uint32_t host_unit() const;
    void     host_selected_unit(uint32_t unit);

    void set_pointer(uint32_t index, int32_t size, uint32_t type, bool normalized, int32_t stride, uint64_t pointer,
                     Mode mode);
    void set_enabled(uint32_t index, bool enabled);
    void set_host_enabled(uint32_t index, bool enabled);
    void clear_slot(uint32_t index);

    uint32_t scratch() const;
    void     set_scratch(uint32_t buffer);
    void     forget_scratch();

    bool     indexed() const;
    void     set_indexed(bool indexed);
    uint32_t validate(uint32_t slot, int32_t size, uint32_t type, int32_t stride) const;
    uint8_t  *staging(uint64_t bytes);
    uint64_t *offset_table(uint32_t count);

    void push(uint32_t mask);
    void pop();
    void reset(uint32_t mask);

    bool invariants_hold() const;

private:
    struct Saved {
        uint32_t array_;
        Arrays   arrays_;
    };

    Arrays *arrays_of(uint32_t array);
    void    refresh(Arrays &arrays, uint32_t index) const;

    KeyedSlots<uint32_t, Arrays> arrays_;
    Arrays                       default_;
    Arrays                      *current_;
    uint32_t                     bound_;
    uint32_t                     unit_;
    uint32_t                     host_unit_;
    uint32_t                     scratch_;
    GrowableArray<Saved>         saved_;
    GrowableArray<uint8_t>       staging_;
    GrowableArray<uint64_t>      offsets_;
    uint64_t                     saved_bits_;
    uint32_t                     depth_;
    bool                         indexed_;

    friend struct ClientVertexStateProbe;
};

}

#endif
