/* One virtio-gpu-rvk device instance: the host GPU it is bound to, the capset
 * it offers, and the contexts and blobs the guest has created on it. The base virtio-gpu device in QEMU
 * handles the 2D protocol; this object owns what the base class does not. */
#ifndef AV_VIRTIO_GPU_RVK_HPP
#define AV_VIRTIO_GPU_RVK_HPP

#include "core/support/status.hpp"
#include "device/capset/device_capset.hpp"
#include "device/context/device_context_table.hpp"
#include "device/execution/executor_library.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/gpu/gpu_binding.hpp"
#include "device/memory/blob_table.hpp"

#include <stdint.h>

namespace av {

class Host;
class TextBuffer;
struct VirtioGpuRvkProbe;

class VirtioGpuRvk {
public:
    /* VIRTIO_CONFIG_S_DRIVER_OK. */
    static constexpr uint8_t driver_ok = 4u;

    explicit VirtioGpuRvk(Host &host);
    VirtioGpuRvk(const VirtioGpuRvk &) = delete;
    VirtioGpuRvk &operator=(const VirtioGpuRvk &) = delete;

    /* Main loop. invalid_state once realized; invalid_parameter, named in
     * `message`, when the base class's own blob support was switched on (the
     * core answers blob requests) or the executor table is not one this ABI
     * accepts; otherwise the binding's answer. Without an executor there is
     * no capset. */
    [[nodiscard]] Status realize(const char *host_gpu, bool base_blobs, const RvkExecutorOps *executor,
                                 TextBuffer &message);
    /* Main loop: a virtio reset destroys every context, releases the fence
     * interrupts, then every blob. */
    void                 reset();
    /* Main loop: the driver's status. DRIVER_OK connects the fence interrupts
     * of a realized device; a failure is logged and leaves them off. */
    void                 status(uint8_t status);
    /* Main loop. Whether the core answered the control request; false leaves it
     * to the base class, as does every request to an unrealized device. */
    bool                 control(void *request, uint32_t type);
    /* How many capsets the device offers: one once realized. */
    uint32_t             capsets() const;
    uint32_t             blobs() const;
    [[nodiscard]] Status create_context(uint32_t id);
    [[nodiscard]] Status destroy_context(uint32_t id);
    uint32_t             contexts() const;
    bool                 realized() const;
    uint32_t             gpu_index() const;

    bool invariants_hold() const;

private:
    Host              &host_;
    GpuBinding         binding_;
    ExecutorLibrary    library_;
    DeviceCapset       capset_;
    /* Before the contexts, which unpin blobs and stop raising interrupts as
     * they are destroyed. */
    BlobTable          blobs_;
    FenceInterrupts    interrupts_;
    DeviceContextTable contexts_;

    friend struct VirtioGpuRvkProbe;
};

}  // namespace av

#endif  // AV_VIRTIO_GPU_RVK_HPP
