/* The resources one host GPU's Direct3D sessions share, by the key the guest
 * shares them under - the creator's process and resource, which every process
 * that opens the guest resource names the same way.
 *
 * The table holds a reference of its own to each, so a resource another
 * process has not opened yet is still there to open. Only the creator lets it
 * go: a process that opened it - DWM, which opens every window's surface -
 * destroys its own handle to the same host object, and taking the key out
 * then left the window's creator drawing into a surface no DWM could open
 * again.
 *
 * Let go is not gone. On Windows a shared surface lives while any process
 * holds it, and a window that replaces its surfaces - a web page refreshed
 * over and over - destroys each one as DWM is still opening it; refused, DWM
 * lost its device and crashed. So a resource its creator released stays open
 * to find for a while, and goes on the next change to the table after that,
 * or first when the table is full.
 *
 * Guarded by the host device's lock, which every session holds around every
 * command. Times are the caller's monotonic nanoseconds. */
#ifndef AV_D3D_SHARED_RESOURCES_HPP
#define AV_D3D_SHARED_RESOURCES_HPP

#include <stdint.h>

struct IUnknown;

namespace av {

class D3dSharedResources {
public:
    static constexpr uint32_t capacity = 4096u;
    /* How long a released resource can still be opened: far longer than a
     * compositor takes to open a surface it was just handed. */
    static constexpr uint64_t released_for_ns = 2000000000u;

    D3dSharedResources();
    D3dSharedResources(const D3dSharedResources &) = delete;
    D3dSharedResources &operator=(const D3dSharedResources &) = delete;

    /* Shares `resource` under `key` for `owner`, the session that made it -
     * compared, never followed - taking a reference. False for a key of 0, a
     * key another resource is shared under and not released, and a full
     * table with nothing released to make room. */
    [[nodiscard]] bool publish(uint64_t key, IUnknown *resource, const void *owner, uint64_t now);
    /* What `key` shares, released or not, with a reference the caller owns;
     * null for none. */
    IUnknown          *find(uint64_t key) const;
    /* `owner` destroyed `resource`: released, if `owner` shared it. Another
     * session destroying its handle to the same object leaves it. */
    void               withdraw(IUnknown *resource, const void *owner, uint64_t now);
    /* The session `owner` ended: everything it shared is released. */
    void               withdraw_all(const void *owner, uint64_t now);
    void               clear();
    uint32_t           count() const;

private:
    static constexpr uint64_t held = ~uint64_t{0};

    /* Takes out what was released longer ago than released_for_ns. */
    void expire(uint64_t now);
    /* The index of the entry released first, or capacity for none. */
    uint32_t oldest_released() const;
    void remove(uint32_t index);

    uint64_t    keys_[capacity];
    IUnknown   *resources_[capacity];
    const void *owners_[capacity];
    /* When the creator released it; held while it has not. */
    uint64_t    released_at_[capacity];
    uint32_t    count_;
};

}  // namespace av

#endif  // AV_D3D_SHARED_RESOURCES_HPP
