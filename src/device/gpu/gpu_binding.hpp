/* Which host GPU a device instance runs on, named by its host-gpu property:
 * "auto" (or no value) binds the first Vulkan physical device, a UUID the one
 * whose deviceUUID matches. A failure explains itself in the message QEMU shows
 * when realize fails, and in the log. */
#ifndef AV_GPU_BINDING_HPP
#define AV_GPU_BINDING_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class TextBuffer;

class GpuBinding {
public:
    explicit GpuBinding(Host &host);
    GpuBinding(const GpuBinding &) = delete;
    GpuBinding &operator=(const GpuBinding &) = delete;

    /* invalid_parameter for a selection that is neither auto nor a UUID;
     * not_found when no host device matches. */
    [[nodiscard]] Status bind(const char *selection, TextBuffer &message);
    bool                 bound() const;
    /* The bound device's position in the host's enumeration. */
    uint32_t             index() const;

private:
    [[nodiscard]] Status bind_uuid(const char *selection, TextBuffer &message);
    [[nodiscard]] Status refuse(Status status, TextBuffer &message);
    bool                 is_auto(const char *selection) const;

    Host    &host_;
    uint32_t index_;
    bool     bound_;
};

}  // namespace av

#endif  // AV_GPU_BINDING_HPP
