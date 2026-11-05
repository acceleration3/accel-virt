#ifndef AV_PROTOCOL_VERSION_HPP
#define AV_PROTOCOL_VERSION_HPP

#include <stdint.h>

namespace av {

class ProtocolVersion {
public:
    static constexpr uint16_t current_major = 2u;
    static constexpr uint16_t current_minor = 0u;

    ProtocolVersion(uint16_t major_version, uint16_t minor_version);

    uint16_t major_version() const;
    uint16_t minor_version() const;

    bool            compatible_with(const ProtocolVersion &peer) const;
    ProtocolVersion negotiated_with(const ProtocolVersion &peer) const;

private:
    uint16_t major_;
    uint16_t minor_;
};

}

#endif
