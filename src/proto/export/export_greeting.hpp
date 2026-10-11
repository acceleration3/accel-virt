/* What a device's export socket answers hello with (docs/external-export.md):
 * which host GPU its surfaces live on - the Vulkan device UUID, and the PCI
 * address where the GPU has one - so a consumer imports on the same GPU. */
#ifndef AV_EXPORT_GREETING_HPP
#define AV_EXPORT_GREETING_HPP

#include <stdint.h>

namespace av {

class ExportGreeting {
public:
    static constexpr uint32_t uuid_bytes = 16u;

    ExportGreeting();
    /* A GPU with no PCI address the host could read. */
    explicit ExportGreeting(const uint8_t *uuid);
    /* A GPU at `domain`:`bus`:`device`.`function`. */
    ExportGreeting(const uint8_t *uuid, uint32_t domain, uint8_t bus, uint8_t device, uint8_t function);

    const uint8_t *uuid() const;
    bool           on_pci() const;
    uint32_t       domain() const;
    uint8_t        bus() const;
    uint8_t        device() const;
    uint8_t        function() const;
    /* A GPU's UUID: what an ok hello must carry, since a consumer that
     * cannot match one cannot know which GPU to import on. */
    bool           valid() const;
    bool           empty() const;

private:
    uint8_t  uuid_[uuid_bytes];
    bool     on_pci_;
    uint32_t domain_;
    uint8_t  bus_;
    uint8_t  device_;
    uint8_t  function_;
};

}  // namespace av

#endif  // AV_EXPORT_GREETING_HPP
