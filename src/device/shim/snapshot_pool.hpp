/* The present snapshots one host GPU's Direct3D 11 device keeps for the
 * present engine (docs/protocol.md, "Present engine"): a blt-model present's
 * pixels, copied on the GPU into a staging texture in the order of the
 * commands that drew them (the Direct3D command PresentSnapshot), under the
 * key the guest chose, and read by the engine's thread once the copy is done.
 * The staging textures are kept and reused; a snapshot nobody read is let go,
 * oldest first, when a new one finds no room. Every call is made under the
 * device's lock. */
#ifndef AV_SNAPSHOT_POOL_HPP
#define AV_SNAPSHOT_POOL_HPP

#include "device/abi/rvk_executor.h"

#include <stdint.h>

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3D11Texture2D;

namespace av {

class SnapshotPool {
public:
    /* More than the presents the engine holds outstanding at once. */
    static constexpr uint32_t capacity = 16u;

    enum class Reading { mapped, copying, absent };

    SnapshotPool();
    ~SnapshotPool();
    SnapshotPool(const SnapshotPool &) = delete;
    SnapshotPool &operator=(const SnapshotPool &) = delete;

    /* Subresource `subresource` of `source`, `width` by `height` pixels of
     * `format`, copied into a staging texture kept under `key`, and the copy
     * flushed to the GPU. False for key 0 or one already kept, or a staging
     * texture the device will not make. */
    [[nodiscard]] bool take(ID3D11Device5 *device, ID3D11DeviceContext4 *context, uint64_t key,
                            ID3D11Texture2D *source, uint32_t subresource, uint32_t width, uint32_t height,
                            uint32_t format);
    /* The snapshot under `key` mapped into `view` once the GPU has copied it:
     * copying until then, absent when none is kept under the key or it is
     * already mapped. */
    Reading              read(ID3D11DeviceContext4 *context, uint64_t key, RvkSnapshotView &view);
    /* The snapshot under `key` unmapped if it was, and its texture free. */
    void                 release(ID3D11DeviceContext4 *context, uint64_t key);
    /* Every snapshot let go, and every texture: the guest's device was reset. */
    void                 clear(ID3D11DeviceContext4 *context);

private:
    struct Slot {
        uint64_t         key_;
        ID3D11Texture2D *texture_;
        uint32_t         width_;
        uint32_t         height_;
        uint32_t         format_;
        uint64_t         taken_;
        bool             mapped_;
    };

    Slot *find(uint64_t key);
    /* A slot for a snapshot of this shape: a free one that already has such
     * a texture, else any free one, else the oldest unmapped snapshot's, which
     * is let go. Null when every slot is mapped. */
    Slot *room(ID3D11DeviceContext4 *context, uint32_t width, uint32_t height, uint32_t format);
    void  forget(ID3D11DeviceContext4 *context, Slot &slot);

    Slot     slots_[capacity];
    uint64_t taken_;
};

}  // namespace av

#endif  // AV_SNAPSHOT_POOL_HPP
