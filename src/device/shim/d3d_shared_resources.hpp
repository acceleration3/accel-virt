/* The resources one host GPU's Direct3D sessions share, by the id the guest
 * shares them under - the one the driver minted over the resource's allocation,
 * which every process that opens it is handed the same.
 *
 * The table holds a reference of its own to each from the moment a session
 * shares it until the guest lets go of it everywhere: dxgkrnl destroys a
 * shared allocation only once every process that made or opened it has, and
 * the driver then tells the device (SharedReleaseRequest). Nothing a session
 * does takes it out - on Windows a surface outlives its creator's handle for
 * as long as DWM holds it, and a page refreshed over and over in the Store
 * destroys each surface as DWM is still opening it.
 *
 * Guarded by the host device's lock, which every session holds around every
 * command. */
#ifndef AV_D3D_SHARED_RESOURCES_HPP
#define AV_D3D_SHARED_RESOURCES_HPP

#include <stdint.h>

struct IUnknown;

namespace av {

class D3dSharedResources {
public:
    static constexpr uint32_t capacity = 4096u;

    D3dSharedResources();
    D3dSharedResources(const D3dSharedResources &) = delete;
    D3dSharedResources &operator=(const D3dSharedResources &) = delete;

    /* Shares `resource` under `key`, taking a reference. False for a key of 0,
     * a key already shared, and a full table. */
    [[nodiscard]] bool publish(uint64_t key, IUnknown *resource);
    /* What `key` shares, with a reference the caller owns; null for none. */
    IUnknown          *find(uint64_t key) const;
    /* The guest let go of `key` everywhere: the table's reference goes. A key
     * it does not hold is nothing to do. */
    void               release(uint64_t key);
    /* Every reference goes: the guest's ids start again. */
    void               clear();
    uint32_t           count() const;

private:
    void remove(uint32_t index);

    uint64_t  keys_[capacity];
    IUnknown *resources_[capacity];
    uint32_t  count_;
};

}  // namespace av

#endif  // AV_D3D_SHARED_RESOURCES_HPP
