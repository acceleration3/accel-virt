/* The fds this context exported from its host objects, and what an import may
 * name (feature 047, US3, research.md §10).
 *
 * A guest device is a host device, so two guest devices sharing an image share
 * it between two host devices - which is what the host's own fd extensions do,
 * inside the one process both live in. The guest's ICD therefore turns a Win32
 * handle into an import by asking for a fresh fd of the object the handle
 * names, and every fd an import carries is one this table handed out. An fd a
 * guest made up names something else in this process entirely (constitution
 * V), so it is refused rather than imported.
 *
 * The table also owns what it exported: an fd the guest asked for and never
 * imported is closed with the device, and with the context, because a handle
 * this process holds until it exits is a leak a guest could drive. There is
 * no count past which an export is refused (docs/architecture.md, "Counts
 * with no fixed bound"). Per context, on that context's own thread, so it
 * needs no lock. */
#ifndef AV_HOST_EXPORTED_FDS_HPP
#define AV_HOST_EXPORTED_FDS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class BlockSource;
class OwnedFd;
struct HostExportedFdsProbe;

class HostExportedFds {
public:
    /* As many as the guest exports: the table grows from `blocks`. */
    explicit HostExportedFds(BlockSource &blocks);
    ~HostExportedFds();
    HostExportedFds(const HostExportedFds &) = delete;
    HostExportedFds &operator=(const HostExportedFds &) = delete;

    /* `descriptor` remembered as `device`'s, and taken from it.
     * invalid_parameter for no descriptor or no device, no_memory when the
     * table cannot grow - the descriptor then stays held there. */
    [[nodiscard]] Status keep(VkDevice device, OwnedFd &descriptor);
    /* Whether `fd` is one this context exported, which is what an import may
     * name. */
    bool                 holds(int fd) const;
    /* The same, and forgotten with it: the driver an import succeeded on owns
     * the fd it was given, so this table must not close it after. A refused
     * import leaves the fd here, to be closed with its device. */
    [[nodiscard]] bool   take(int fd);
    /* Every fd `device` exported and nothing imported, closed with it. */
    void                 forget_device(VkDevice device);
    /* Every fd, closed, whichever device exported it: a closing context's,
     * which the next context made in its slot must not be able to name. */
    void                 forget_all();
    uint32_t             live() const;

    /* Every fd held is a descriptor of one device. */
    bool invariants_hold() const;

private:
    /* Each fd held, by its number, with the device that exported it. */
    KeyedSlots<uint32_t, VkDevice> fds_;

    friend struct HostExportedFdsProbe;
};

}  // namespace av

#endif  // AV_HOST_EXPORTED_FDS_HPP
