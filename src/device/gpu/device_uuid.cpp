#include "device/gpu/device_uuid.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DeviceUuid::DeviceUuid() : bytes_{}
{
    AV_SCOPE("DeviceUuid::DeviceUuid");
}

int32_t DeviceUuid::digit(char c) const
{
    AV_SCOPE("DeviceUuid::digit");
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool DeviceUuid::separator_at(uint32_t position) const
{
    AV_SCOPE("DeviceUuid::separator_at");
    return position == 8u || position == 13u || position == 18u || position == 23u;
}

Status DeviceUuid::parse(const char *text)
{
    AV_SCOPE("DeviceUuid::parse");
    if (text == nullptr) return Status::invalid_parameter;
    uint8_t  parsed[bytes] = {};
    uint32_t nibbles = 0u;
    for (uint32_t position = 0u; position < text_length; position++) {
        const char c = text[position];
        if (separator_at(position) != (c == '-')) return Status::invalid_parameter;
        if (c == '-') continue;
        const int32_t value = digit(c);
        if (value < 0) return Status::invalid_parameter;
        parsed[nibbles / 2u] = static_cast<uint8_t>(uint32_t{parsed[nibbles / 2u]} << 4u | static_cast<uint32_t>(value));
        nibbles++;
    }
    if (text[text_length] != '\0') return Status::invalid_parameter;
    for (uint32_t index = 0u; index < bytes; index++) bytes_[index] = parsed[index];
    return Status::ok;
}

bool DeviceUuid::equals(const uint8_t *uuid) const
{
    AV_SCOPE("DeviceUuid::equals");
    uint8_t difference = 0u;
    for (uint32_t index = 0u; index < bytes; index++) difference |= static_cast<uint8_t>(bytes_[index] ^ uuid[index]);
    return difference == 0u;
}

}  // namespace av
