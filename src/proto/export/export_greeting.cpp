#include "proto/export/export_greeting.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ExportGreeting::ExportGreeting()
    : uuid_{}, capacity_(0u), on_pci_(false), domain_(0u), bus_(0u), device_(0u), function_(0u)
{
    AV_SCOPE("ExportGreeting::ExportGreeting");
}

ExportGreeting::ExportGreeting(const uint8_t *uuid, uint32_t capacity) : ExportGreeting()
{
    AV_SCOPE("ExportGreeting::ExportGreeting");
    for (uint32_t index = 0u; index < uuid_bytes; index++) uuid_[index] = uuid[index];
    capacity_ = capacity;
}

ExportGreeting::ExportGreeting(const uint8_t *uuid, uint32_t capacity, uint32_t domain, uint8_t bus, uint8_t device,
                               uint8_t function)
    : ExportGreeting(uuid, capacity)
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

uint32_t ExportGreeting::capacity() const
{
    AV_SCOPE("ExportGreeting::capacity");
    return capacity_;
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

/* Off PCI, no constructor sets an address field, so a capacity is all there
 * is to check. */
bool ExportGreeting::valid() const
{
    AV_SCOPE("ExportGreeting::valid");
    return capacity_ != 0u;
}

bool ExportGreeting::empty() const
{
    AV_SCOPE("ExportGreeting::empty");
    bool blank = capacity_ == 0u && !on_pci_;
    for (const uint8_t byte : uuid_) blank = blank && byte == 0u;
    return blank;
}

}  // namespace av
