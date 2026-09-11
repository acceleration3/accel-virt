/* One virtio-gpu-rvk device instance: the host GPU it is bound to and the
 * contexts the guest has created on it. The base virtio-gpu device in QEMU
 * handles the 2D protocol; this object owns what the base class does not. */
#ifndef AV_VIRTIO_GPU_RVK_HPP
#define AV_VIRTIO_GPU_RVK_HPP

#include "core/support/status.hpp"
#include "device/context/device_context_table.hpp"
#include "device/gpu/gpu_binding.hpp"

#include <stdint.h>

namespace av {

class Host;
class TextBuffer;

class VirtioGpuRvk {
public:
    explicit VirtioGpuRvk(Host &host);
    VirtioGpuRvk(const VirtioGpuRvk &) = delete;
    VirtioGpuRvk &operator=(const VirtioGpuRvk &) = delete;

    /* Main loop. invalid_state once realized; otherwise the binding's answer. */
    [[nodiscard]] Status realize(const char *host_gpu, TextBuffer &message);
    /* Main loop: a virtio reset destroys every context. */
    void                 reset();
    [[nodiscard]] Status create_context(uint32_t id);
    [[nodiscard]] Status destroy_context(uint32_t id);
    uint32_t             contexts() const;
    bool                 realized() const;
    uint32_t             gpu_index() const;

    bool invariants_hold() const;

private:
    Host              &host_;
    GpuBinding         binding_;
    DeviceContextTable contexts_;
};

}  // namespace av

#endif  // AV_VIRTIO_GPU_RVK_HPP
