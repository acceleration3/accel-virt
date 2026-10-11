#include "proto/export/export_greeting.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ExportGreeting::ExportGreeting()
    : uuid_{}, on_pci_(false), domain_(0u), bus_(0u), device_(0u), function_(0u)
{
    AV_SCOPE("ExportGreeting::ExportGreeting");
}

ExportGreeting::ExportGreeting(const uint8_t *uuid) : ExportGreeting()
{
    AV_SCOPE("ExportGreeting::ExportGreeting");
    for (uint32_t index = 0u; index < uuid_bytes; index++) uuid_[index] = uuid[index];
}

ExportGreeting::ExportGreeting(const uint8_t *uuid, uint32_t domain, uint8_t bus, uint8_t device, uint8_t function)
    : ExportGreeting(uuid)
{
    AV_SCOPE("ExportGreeting::ExportGreeting");
    on_pci_ = true;
    domain_ = domain;
    bus_ = bus;
    device_ = device;
    function_ = function;
}

const uint8_t *ExportGreeting::uuid() const
{
    AV_SCOPE("ExportGreeting::uuid");
    return uuid_;
}

bool ExportGreeting::on_pci() const
{
    AV_SCOPE("ExportGreeting::on_pci");
    return on_pci_;
}

uint32_t ExportGreeting::domain() const
{
    AV_SCOPE("ExportGreeting::domain");
    return domain_;
}

uint8_t ExportGreeting::bus() const
{
    AV_SCOPE("ExportGreeting::bus");
    return bus_;
}

uint8_t ExportGreeting::device() const
{
    AV_SCOPE("ExportGreeting::device");
    return device_;
}

uint8_t ExportGreeting::function() const
{
    AV_SCOPE("ExportGreeting::function");
    return function_;
}

/* Off PCI, no constructor sets an address field, so the UUID is all there is
 * to check. */
bool ExportGreeting::valid() const
{
    AV_SCOPE("ExportGreeting::valid");
    for (const uint8_t byte : uuid_)
        if (byte != 0u) return true;
    return false;
}

bool ExportGreeting::empty() const
{
    AV_SCOPE("ExportGreeting::empty");
    return !on_pci_ && !valid();
}

}  // namespace av
