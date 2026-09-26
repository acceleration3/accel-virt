/* Where a device's contexts run: in QEMU's own process, or in a renderer
 * process that a crash in the host GPU stack takes down alone
 * (docs/renderer.md). The device's control plane - its commands, blobs,
 * heads and interrupts - asks this for everything that touches an executor.
 * Main loop only. */
#ifndef AV_CONTEXT_PLANE_HPP
#define AV_CONTEXT_PLANE_HPP

#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {

class CapsetInfo;
class ContextSetup;
class StateWriter;

class ContextPlane {
public:
    /* The executor library's table: invalid_parameter for one this ABI does
     * not accept. */
    [[nodiscard]] virtual Status accept(const RvkExecutorOps *executor) = 0;
    /* The locks the contexts share: no_memory when the host has none. */
    [[nodiscard]] virtual Status initialize() = 0;
    /* Where the contexts run is made ready for GPU `gpu_uuid` and a capset
     * offering at least what the one the guest reads will: the renderer is
     * started and greeted. Whatever went wrong, named for the log. */
    [[nodiscard]] virtual Status start(const uint8_t *gpu_uuid, const CapsetInfo &capset) = 0;
    virtual uint32_t             sub_apis() const = 0;
    virtual uint32_t             import_kinds(SubApi api, const uint8_t *gpu_uuid) const = 0;

    /* invalid_parameter for id 0, conflict for a live id, no_space when full. */
    [[nodiscard]] virtual Status create(uint32_t id) = 0;
    [[nodiscard]] virtual Status destroy(uint32_t id) = 0;
    /* Every context, then every object they shared. */
    virtual void                 destroy_all() = 0;
    virtual bool                 holds(uint32_t id) = 0;
    /* not_found for a resource the setup names that is not there, no_memory,
     * or invalid_parameter for anything else refused. */
    [[nodiscard]] virtual Status bind(uint32_t id, const ContextSetup &setup, const CapsetInfo &capset) = 0;
    virtual bool                 holds_doorbell(uint32_t index) = 0;
    virtual bool                 can_share(uint32_t asking, uint32_t share) = 0;
    virtual uint32_t             live() const = 0;

    /* The device's blob table changed, or its fence interrupts: contexts in
     * another process are told, since they hold a copy of what they read. */
    virtual void blob_created(uint32_t id) = 0;
    virtual void blob_released(uint32_t id) = 0;
    virtual void interrupts_changed(bool connected) = 0;
    /* The process the contexts ran in died: they are lost, and it is
     * started again. */
    virtual void renderer_lost() = 0;

    /* After every Direct3D stream has run what it published. */
    virtual bool export_image(const uint8_t *gpu_uuid, uint64_t key, RvkExportedImage &image) = 0;
    virtual bool read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                            uint32_t height) = 0;

    virtual void report(StateWriter &writer) = 0;
    virtual bool invariants_hold() const = 0;

protected:
    ContextPlane() = default;
    ~ContextPlane() = default;
    ContextPlane(const ContextPlane &) = delete;
    ContextPlane &operator=(const ContextPlane &) = delete;
};

}  // namespace av

#endif  // AV_CONTEXT_PLANE_HPP
