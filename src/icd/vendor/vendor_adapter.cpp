#include "icd/vendor/vendor_adapter.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

namespace av::vendor {

VendorAdapter::VendorAdapter()
    : luid_(0u), identity_(), location_(), name_(), driver_version_(0u), dedicated_video_(0u), dedicated_system_(0u),
      shared_system_(0u), integrated_(false), primary_(false)
{
    AV_SCOPE("VendorAdapter::VendorAdapter");
}

VendorAdapter::VendorAdapter(uint64_t luid, const PciIdentity &identity, const PciLocation &location,
                             uint64_t driver_version, uint64_t dedicated_video, uint64_t dedicated_system,
                             uint64_t shared_system, bool integrated, bool primary)
    : luid_(luid), identity_(identity), location_(location), name_(), driver_version_(driver_version),
      dedicated_video_(dedicated_video), dedicated_system_(dedicated_system), shared_system_(shared_system),
      integrated_(integrated), primary_(primary)
{
    AV_SCOPE("VendorAdapter::VendorAdapter");
}

void VendorAdapter::take_name(const uint16_t *wide)
{
    AV_SCOPE("VendorAdapter::take_name");
    name_.take(wide);
}

uint64_t           VendorAdapter::luid() const
{
    AV_SCOPE("VendorAdapter::luid");
    return luid_;
}
const PciIdentity &VendorAdapter::identity() const
{
    AV_SCOPE("VendorAdapter::identity");
    return identity_;
}
const PciLocation &VendorAdapter::location() const
{
    AV_SCOPE("VendorAdapter::location");
    return location_;
}
const char        *VendorAdapter::name() const
{
    AV_SCOPE("VendorAdapter::name");
    return name_.text();
}
uint64_t           VendorAdapter::driver_version() const
{
    AV_SCOPE("VendorAdapter::driver_version");
    return driver_version_;
}
void               VendorAdapter::append_driver_version(TextBuffer &text) const
{
    AV_SCOPE("VendorAdapter::append_driver_version");
    constexpr uint32_t parts = 4u;
    for (uint32_t part = 0u; part < parts; ++part) {
        if (part != 0u) text.append(".");
        text.append_decimal((driver_version_ >> (16u * (parts - 1u - part))) & 0xFFFFu);
    }
}
uint64_t           VendorAdapter::dedicated_video_bytes() const
{
    AV_SCOPE("VendorAdapter::dedicated_video_bytes");
    return dedicated_video_;
}
uint64_t           VendorAdapter::dedicated_system_bytes() const
{
    AV_SCOPE("VendorAdapter::dedicated_system_bytes");
    return dedicated_system_;
}
uint64_t           VendorAdapter::shared_system_bytes() const
{
    AV_SCOPE("VendorAdapter::shared_system_bytes");
    return shared_system_;
}
bool               VendorAdapter::integrated() const
{
    AV_SCOPE("VendorAdapter::integrated");
    return integrated_;
}
bool               VendorAdapter::primary() const
{
    AV_SCOPE("VendorAdapter::primary");
    return primary_;
}

}  // namespace av::vendor
