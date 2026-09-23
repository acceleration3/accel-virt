/* The resources one host GPU's Direct3D sessions share, by the key the guest
 * shares them under - the KMD's shared-object id, which every process that
 * opens the guest resource names the same way.
 *
 * The table holds a reference of its own to each, from the moment the
 * creating session shares it until that session destroys it, so a resource
 * another process has not opened yet is still there to open. Guarded by the
 * host device's lock, which every session holds around every command. */
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
    /* Stops sharing `resource`, under whatever key: its creator destroyed it. */
    void               withdraw(IUnknown *resource);
    void               clear();
    uint32_t           count() const;

private:
    uint64_t  keys_[capacity];
    IUnknown *resources_[capacity];
    uint32_t  count_;
};

}  // namespace av

#endif  // AV_D3D_SHARED_RESOURCES_HPP
