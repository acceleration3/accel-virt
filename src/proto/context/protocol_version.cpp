#include "proto/context/protocol_version.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ProtocolVersion::ProtocolVersion(uint16_t major_version, uint16_t minor_version)
    : major_(major_version), minor_(minor_version)
    {
        AV_SCOPE("ProtocolVersion::ProtocolVersion");
    }

uint16_t ProtocolVersion::major_version() const
{
    AV_SCOPE("ProtocolVersion::major_version");
    return major_;
}
uint16_t ProtocolVersion::minor_version() const
{
    AV_SCOPE("ProtocolVersion::minor_version");
    return minor_;
}

bool ProtocolVersion::compatible_with(const ProtocolVersion &peer) const
{
    AV_SCOPE("ProtocolVersion::compatible_with");
    return major_ == peer.major_;
}

ProtocolVersion ProtocolVersion::negotiated_with(const ProtocolVersion &peer) const
{
    AV_SCOPE("ProtocolVersion::negotiated_with");
    return ProtocolVersion(major_, minor_ < peer.minor_ ? minor_ : peer.minor_);
}

}
