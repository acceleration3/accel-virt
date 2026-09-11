/* The virtio-gpu control commands for contexts the device core answers
 * (docs/protocol.md): the capset queries, context creation and destruction,
 * and SUBMIT_3D carrying a context setup. Every field is checked before a
 * context changes; each answer is one response. Main loop. */
#ifndef AV_CONTEXT_COMMANDS_HPP
#define AV_CONTEXT_COMMANDS_HPP

#include <stdint.h>

namespace av {

class ControlRequest;
class DeviceCapset;
class DeviceContextTable;
class GpuResponse;
class Host;

class ContextCommands {
public:
    enum : uint32_t {   /* VIRTIO_GPU_CMD_* */
        get_capset_info = 0x0108u,
        get_capset = 0x0109u,
        ctx_create = 0x0200u,
        ctx_destroy = 0x0201u,
        submit_3d = 0x0207u,
    };

    ContextCommands(Host &host, DeviceContextTable &contexts, const DeviceCapset &capset);
    ContextCommands(const ContextCommands &) = delete;
    ContextCommands &operator=(const ContextCommands &) = delete;

    /* Whether the core answered `request`; false leaves it to the base class. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    void     capset_info(ControlRequest &request, GpuResponse &response);
    void     capset(ControlRequest &request, GpuResponse &response);
    uint32_t create(ControlRequest &request);
    uint32_t destroy(ControlRequest &request);
    uint32_t submit(ControlRequest &request);

    Host               &host_;
    DeviceContextTable &contexts_;
    const DeviceCapset &capset_;
};

}  // namespace av

#endif  // AV_CONTEXT_COMMANDS_HPP
