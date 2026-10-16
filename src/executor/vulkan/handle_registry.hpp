/* Every host handle this executor made, and nothing else.
 *
 * A handle crosses the wire as a number, and on the host that number is an
 * attacker's: the generated decoder used to hand it to the driver as a pointer,
 * so a guest naming any address had it dereferenced. Found by
 * tests/fuzz/fuzz_executor_vulkan.cpp on its first run, in vkDestroyInstance.
 *
 * So the host answers a handle only if it made it. Everything a guest can name
 * is in here, put there when the command that created it succeeded and taken
 * out when the command that destroyed it ran. A number that is not is not a
 * handle: it is a frame this host refuses.
 *
 * Taken out with it is everything made through it. A driver that destroys a
 * device frees the device's buffers, and a buffer this registry still knew
 * would be a freed pointer a guest could name (feature 044, T031). */
#ifndef AV_HANDLE_REGISTRY_HPP
#define AV_HANDLE_REGISTRY_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

/* What a handle is, for the five the loader dispatches on. Everything else is
 * `other`: the loader passes a non-dispatchable handle to the driver as the
 * number it is, while a dispatchable one it dereferences to find the table to
 * call through - so a guest that names a queue where a device belongs aborts
 * the host process rather than getting an error back. Found by
 * tests/fuzz/fuzz_executor_vulkan.cpp in vkAllocateMemory.
 *
 * And an image made with the memory source's handle type, the one kind of
 * image its memory may back (VulkanBackend::on_vkCreateImage): kept with the
 * handle, it ends when the image does. */
enum class HandleKind : uint8_t {
    other,
    instance,
    physical_device,
    device,
    queue,
    command_buffer,
    external_image
};

/* One handle's place: the slot of what it was made through - no_slot for
 * nothing - how many it made, and what it is. */
struct HandleSlot {
    uint32_t   parent_;
    uint32_t   children_;
    HandleKind kind_;
};

class HandleRegistry {
public:
    explicit HandleRegistry(BlockSource &blocks);
    HandleRegistry(const HandleRegistry &) = delete;
    HandleRegistry &operator=(const HandleRegistry &) = delete;
    HandleRegistry(HandleRegistry &&) = delete;
    HandleRegistry &operator=(HandleRegistry &&) = delete;

    /* Room for `handles` more, so remembering that many cannot fail: asked
     * before the driver makes them, so a guest that wants more than the host
     * can hold is told it is out of memory instead of being handed a handle
     * this host would refuse. no_memory when there is none. */
    [[nodiscard]] Status make_room(uint64_t handles);
    /* Remembers `handle`, which this executor just made through `parent`: the
     * null handle or one it does not know for a handle made through nothing.
     * One already known keeps the parent it was made through. no_memory when
     * no room was made for it and none can be. */
    [[nodiscard]] Status remember(uint64_t handle, uint64_t parent, HandleKind kind = HandleKind::other);
    /* Whether this executor made `handle`. The null handle is always known:
     * the API defines it and a guest may legitimately send it. */
    bool                 knows(uint64_t handle) const;
    /* Whether this executor made `handle` as `kind`. A handle it made as
     * something else is no more this one than a number it never made, and
     * the null handle is no kind of handle: no dispatchable one, and no image
     * made for the memory source. */
    bool                 knows(uint64_t handle, HandleKind kind) const;
    /* Forgets one it made, and everything made through it at any depth.
     * Nothing for one it did not. */
    void                 forget(uint64_t handle);
    /* Forgets everything made through `handle`, at any depth, and not
     * `handle` itself. */
    void                 forget_children(uint64_t handle);
    /* Forgets everything and gives the table's memory back. */
    void                 forget_all();

    uint32_t count() const;

    /* Every handle known is no null handle and its parent a live slot other
     * than its own, and the children slots count are the handles naming one. */
    bool invariants_hold() const;

private:
    friend struct HandleRegistryProbe;

    static constexpr uint32_t no_slot = KeyedSlots<uint64_t, HandleSlot>::no_slot;

    void free_slot(uint32_t slot);
    /* Frees every slot whose parent is free, until none is. */
    void sweep_orphans();

    /* A parent is a slot rather than a handle, so finding the orphans of a
     * freed slot is one pass, and `children_` says whether a slot has any to
     * find: forgetting a leaf, which most handles are, touches nothing else. */
    KeyedSlots<uint64_t, HandleSlot> slots_;
};

}  // namespace av

#endif  // AV_HANDLE_REGISTRY_HPP
