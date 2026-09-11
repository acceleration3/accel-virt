#include "protocol_version.hpp"

namespace av {

ProtocolVersion::ProtocolVersion(uint16_t major_version, uint16_t minor_version)
    : major_(major_version), minor_(minor_version) {}

uint16_t ProtocolVersion::major_version() const { return major_; }
uint16_t ProtocolVersion::minor_version() const { return minor_; }

bool ProtocolVersion::compatible_with(const ProtocolVersion &peer) const
{
    return major_ == peer.major_;
}

ProtocolVersion ProtocolVersion::negotiated_with(const ProtocolVersion &peer) const
{
    return ProtocolVersion(major_, minor_ < peer.minor_ ? minor_ : peer.minor_);
}

}  // namespace av
