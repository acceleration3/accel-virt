/* Small dynamic constant buffers, kept in one ring instead of their own
 * storage. A write that discards a whole constant buffer would make DXVK
 * rename it - a fresh buffer slice per write, which a frame of hundreds of
 * per-draw constants spent over a quarter of the worker's time allocating.
 * Here the write is appended to the ring (a no-overwrite map, which renames
 * nothing), and every slot the buffer is bound at is rebound to the ring at
 * the new offset (the D3D11.1 SetConstantBuffers1 window). Bindings are
 * tracked per guest device, whose pipeline states the host swaps between; a
 * device's stale windows are rebound when it is selected again. A buffer is
 * written back to its own storage, from a shadow of its last contents, before
 * anything else reads it. */
#ifndef AV_CONSTANT_RING_HPP
#define AV_CONSTANT_RING_HPP

#include <stdint.h>

struct ID3D11Buffer;
struct ID3D11Device5;
struct ID3D11DeviceContext4;

namespace av {

class ConstantRing {
public:
    static constexpr uint32_t stage_count = 6u;
    static constexpr uint32_t slot_count = 14u;
    /* The largest buffer kept here: a draw's constants, not a scene's. */
    static constexpr uint32_t largest = 4096u;

    ConstantRing();
    ~ConstantRing();
    ConstantRing(const ConstantRing &) = delete;
    ConstantRing &operator=(const ConstantRing &) = delete;

    /* A whole-buffer discard of `buffer` (guest id `id`), taken into the ring;
     * false when the buffer is not one this keeps, and the caller writes it. */
    [[nodiscard]] bool update(ID3D11Device5 *device, ID3D11DeviceContext4 *context, uint32_t device_id,
                              uint32_t id, ID3D11Buffer *buffer, const uint8_t *data, uint32_t bytes);
    /* A bind about to be made on `device_id`: remembered, and a buffer kept
     * here swapped for its window of the ring in `buffers`/`first`/`counts`. */
    void bind(uint32_t device_id, uint32_t stage, uint32_t start, uint32_t count, const uint32_t *ids,
              ID3D11Buffer **buffers, uint32_t *first, uint32_t *counts);
    /* `device_id` is selected again: its windows onto buffers written since
     * are moved to where the buffers are now. */
    void selected(ID3D11DeviceContext4 *context, uint32_t device_id);
    /* The device's pipeline state was cleared: nothing of it is bound. */
    void cleared(uint32_t device_id);
    /* `buffer` is about to be read or written some other way: its storage gets
     * its contents back, and the ring lets it go. */
    void release(ID3D11DeviceContext4 *context, uint32_t device_id, uint32_t id, ID3D11Buffer *buffer);
    /* The buffer is gone. */
    void forget(uint32_t id);
    /* The device is gone. */
    void forget_device(uint32_t device_id);
    /* The ring buffer given back; before the device goes. */
    void close();

private:
    static constexpr uint32_t ring_bytes = 1u << 20u;
    static constexpr uint32_t window_bytes = 256u;
    static constexpr uint32_t bytes_per_constant = 16u;
    static constexpr uint32_t max_buffers = 256u;
    static constexpr uint32_t max_devices = 64u;
    static constexpr uint32_t none = ~0u;

    struct Kept {
        uint32_t id_;
        uint32_t bytes_;
        uint32_t offset_;
        uint8_t  shadow_[largest];
    };
    struct Binding {
        uint32_t id_;
        uint32_t offset_;    /* where the window was put; none for the buffer's own storage */
        uint32_t first_;     /* the guest's own window, in constants */
        uint32_t count_;
        /* The guest's buffer, not held: what the slot goes back to. */
        ID3D11Buffer *buffer_;
    };
    struct DeviceBindings {
        uint32_t device_;
        Binding  slots_[stage_count][slot_count];
    };

    uint32_t        kept_index(uint32_t id) const;
    DeviceBindings *bindings_of(uint32_t device_id, bool make);
    [[nodiscard]] bool write(ID3D11Device5 *device, ID3D11DeviceContext4 *context, Kept &kept);
    void            rebind(ID3D11DeviceContext4 *context, DeviceBindings &device);
    void            set_one(ID3D11DeviceContext4 *context, uint32_t stage, uint32_t slot, ID3D11Buffer *buffer,
                            uint32_t first, uint32_t count) const;
    void            window(const Kept &kept, const Binding &binding, uint32_t &first, uint32_t &count) const;

    ID3D11Buffer   *ring_;
    uint32_t        used_;
    /* The last write discarded the ring: every kept buffer moved. */
    bool            wrapped_;
    Kept            kept_[max_buffers];
    uint32_t        kept_count_;
    DeviceBindings  devices_[max_devices];
    uint32_t        device_count_;
};

}  // namespace av

#endif  // AV_CONSTANT_RING_HPP
