#ifndef AV_EXPORT_GREETING_HPP
#define AV_EXPORT_GREETING_HPP

#include <stdint.h>

namespace av {

class ExportGreeting {
public:
    static constexpr uint32_t uuid_bytes = 16u;

    ExportGreeting();
    explicit ExportGreeting(const uint8_t *uuid);
    ExportGreeting(const uint8_t *uuid, uint32_t domain, uint8_t bus, uint8_t device, uint8_t function);

    const uint8_t *uuid() const;
    bool           on_pci() const;
    uint32_t       domain() const;
    uint8_t        bus() const;
    uint8_t        device() const;
    uint8_t        function() const;
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

}

#endif
