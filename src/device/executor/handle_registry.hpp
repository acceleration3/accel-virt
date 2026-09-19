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

#include <stdint.h>

namespace av {

/* What a handle is, for the five the loader dispatches on. Everything else is
 * `other`: the loader passes a non-dispatchable handle to the driver as the
 * number it is, while a dispatchable one it dereferences to find the table to
 * call through - so a guest that names a queue where a device belongs aborts
 * the host process rather than getting an error back. Found by
 * tests/fuzz/fuzz_executor_vulkan.cpp in vkAllocateMemory. */
enum class HandleKind : uint8_t {
    other,
    instance,
    physical_device,
    device,
    queue,
    command_buffer
};

class HandleRegistry {
public:
    /* A Vulkan context of any size lives well inside this: it is objects the
     * guest created, not commands it sent, and a guest that wants more is
     * refused rather than served. */
    static constexpr uint32_t max_handles = 4096u;

    HandleRegistry();
    HandleRegistry(const HandleRegistry &) = delete;
    HandleRegistry &operator=(const HandleRegistry &) = delete;

    /* Remembers `handle`, which this executor just made through `parent`: the
     * null handle or one it does not know for a handle made through nothing.
     * One already known keeps the parent it was made through. False when full,
     * which the caller turns into a refusal rather than a silent forget. */
    [[nodiscard]] bool remember(uint64_t handle, uint64_t parent, HandleKind kind = HandleKind::other);
    /* Whether this executor made `handle`. The null handle is always known:
     * the API defines it and a guest may legitimately send it. */
    bool               knows(uint64_t handle) const;
    /* Whether this executor made `handle` as `kind`. A handle it made as
     * something else is no more this one than a number it never made. */
    bool               knows(uint64_t handle, HandleKind kind) const;
    /* Forgets one it made, and everything made through it at any depth.
     * Nothing for one it did not. */
    void               forget(uint64_t handle);
    /* Forgets everything made through `handle`, at any depth, and not
     * `handle` itself. */
    void               forget_children(uint64_t handle);
    void               forget_all();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct HandleRegistryProbe;

    /* A slot's parent when it was made through nothing, and a free slot's. */
    static constexpr uint16_t no_parent = 0xFFFFu;

    /* The slot holding `handle`, or `end_` for none. */
    uint32_t slot_of(uint64_t handle) const;
    void     free_slot(uint32_t slot);
    /* Frees every slot whose parent is free, until none is, then gives back
     * the free slots at the end. */
    void     sweep_orphans();

    /* Slots below `end_` hold a handle or zero for a free one, and one slot is
     * the same index in both. A parent is a slot rather than a handle so that
     * finding the orphans of a free slot is one pass, not one search per
     * entry; a slot is only reused once nothing names it as a parent. */
    uint64_t   handles_[max_handles];
    uint16_t   parents_[max_handles];
    HandleKind kinds_[max_handles];
    uint32_t end_;
    uint32_t count_;
};

}  // namespace av

#endif  // AV_HANDLE_REGISTRY_HPP
