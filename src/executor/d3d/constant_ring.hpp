#ifndef AV_CONSTANT_RING_HPP
#define AV_CONSTANT_RING_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

struct ID3D11Buffer;
struct ID3D11Device5;
struct ID3D11DeviceContext4;

namespace av {

class BlockSource;

class ConstantRing {
public:
    static constexpr uint32_t stage_count = 6u;
    static constexpr uint32_t slot_count = 14u;
    static constexpr uint32_t largest = 4096u;

    explicit ConstantRing(BlockSource &blocks);
    ~ConstantRing();
    ConstantRing(const ConstantRing &) = delete;
    ConstantRing &operator=(const ConstantRing &) = delete;

    [[nodiscard]] bool update(ID3D11Device5 *device, ID3D11DeviceContext4 *context, uint32_t device_id,
                              uint32_t id, ID3D11Buffer *buffer, const uint8_t *data, uint32_t bytes);
    [[nodiscard]] Status bind(uint32_t device_id, uint32_t stage, uint32_t start, uint32_t count,
                              const uint32_t *ids, ID3D11Buffer **buffers, uint32_t *first, uint32_t *counts);
    void selected(ID3D11DeviceContext4 *context, uint32_t device_id);
    void cleared(uint32_t device_id);
    void release(ID3D11DeviceContext4 *context, uint32_t device_id, uint32_t id, ID3D11Buffer *buffer);
    void forget(uint32_t id);
    void forget_device(uint32_t device_id);
    void close();

private:
    static constexpr uint32_t ring_bytes = 1u << 20u;
    static constexpr uint32_t window_bytes = 256u;
    static constexpr uint32_t bytes_per_constant = 16u;
    static constexpr uint32_t max_buffers = ring_bytes / largest;
    static constexpr uint32_t none = ~0u;

    struct Kept {
        uint32_t id_;
        uint32_t bytes_;
        uint32_t offset_;
        uint8_t  shadow_[largest];
    };
    struct Binding {
        uint32_t id_;
        uint32_t offset_;
        uint32_t first_;
        uint32_t count_;
        ID3D11Buffer *buffer_;
    };
    struct DeviceBindings {
        Binding slots_[stage_count][slot_count];
    };

    uint32_t        kept_index(uint32_t id) const;
    DeviceBindings *bindings_of(uint32_t device_id);
    [[nodiscard]] bool write(ID3D11Device5 *device, ID3D11DeviceContext4 *context, Kept &kept);
    void            rebind(ID3D11DeviceContext4 *context, DeviceBindings &device);
    void            set_one(ID3D11DeviceContext4 *context, uint32_t stage, uint32_t slot, ID3D11Buffer *buffer,
                            uint32_t first, uint32_t count) const;
    void            window(const Kept &kept, const Binding &binding, uint32_t &first, uint32_t &count) const;

    ID3D11Buffer   *ring_;
    uint32_t        used_;
    bool            wrapped_;
    Kept            kept_[max_buffers];
    uint32_t        kept_count_;
    KeyedSlots<uint32_t, DeviceBindings> devices_;
};

}

#endif
