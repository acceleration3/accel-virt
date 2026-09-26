/* One guest context whose worker runs in the renderer (docs/renderer.md).
 * QEMU still pins the blobs it runs on - its ring, fence page and reply slot -
 * so the guest cannot free pages the renderer reads, and QEMU still owns the
 * doorbell KVM signals, handing the renderer only its descriptor. When the
 * renderer dies, this is what tells the guest: the fence page QEMU can reach
 * says the device was lost, and the context's interrupt is raised. Main loop
 * only. */
#ifndef AV_REMOTE_CONTEXT_HPP
#define AV_REMOTE_CONTEXT_HPP

#include "core/support/status.hpp"
#include "device/context/context_binding.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class CapsetInfo;
class ContextSetup;
class FenceInterrupts;
class Host;
class RendererLink;

class RemoteContext {
public:
    RemoteContext(Host &host, BlobTable &blobs, FenceInterrupts &interrupts, uint32_t id);
    ~RemoteContext();
    RemoteContext(const RemoteContext &) = delete;
    RemoteContext &operator=(const RemoteContext &) = delete;

    /* The binding QEMU checks and pins, then the renderer's. What the bind
     * came to is `outcome`: QEMU's refusal - invalid_state for a context that
     * is bound or lost - the renderer's, or ok. The link's status is
     * returned: not_found, invalid_state or malformed when the renderer is
     * gone, silent or answered what no renderer sends. Nothing stays bound
     * unless both are ok. */
    [[nodiscard]] Status bind(const ContextSetup &setup, const CapsetInfo &capset, RendererLink &link,
                              Status &outcome);
    /* The pins and the doorbell go; the renderer is told by the plane. */
    void                 unbind();
    /* The renderer died: the guest sees its device lost. Bound contexts only;
     * a second loss says nothing more. */
    void                 lose();

    uint32_t id() const;
    bool     bound() const;
    bool     lost() const;
    SubApi   sub_api() const;
    uint32_t doorbell() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status connect_doorbell(uint32_t index, int &descriptor);
    void                 disconnect_doorbell();

    Host            &host_;
    FenceInterrupts &interrupts_;
    ContextBinding   binding_;
    uint32_t         id_;
    void            *doorbell_event_;
    bool             lost_;
};

}  // namespace av

#endif  // AV_REMOTE_CONTEXT_HPP
