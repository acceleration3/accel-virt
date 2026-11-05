#include "core/support/status.hpp"
#include "device/context/context_label.hpp"
#include "proto/virtio_gpu/context_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

void check_request(const std::uint8_t *data, std::size_t size)
{
    av::WireReader     reader(data, size);
    av::ContextRequest request;
    if (request.decode_create(reader) != av::Status::ok) {
        if (request.name()[0] != '\0' || request.name_length() != 0u) __builtin_trap();
        return;
    }
    const char *const name = request.name();
    std::size_t       length = 0u;
    while (length <= av::ContextRequest::name_capacity && name[length] != '\0') {
        const auto byte = static_cast<unsigned char>(name[length]);
        if (byte < 0x20u || byte >= 0x7Fu) __builtin_trap();
        length++;
    }
    if (length > av::ContextRequest::name_capacity) __builtin_trap();
    if (length > request.name_length()) __builtin_trap();
    std::uint32_t claimed = 0u;
    std::memcpy(&claimed, data + 24, sizeof claimed);
    if (request.name_length() != claimed) __builtin_trap();
    for (std::size_t at = 0u; at < length; at++) {
        const std::uint8_t raw = data[32u + at];
        const bool         printable = raw >= 0x20u && raw < 0x7Fu;
        if (raw == 0u || (printable && name[at] != static_cast<char>(raw)) || (!printable && name[at] != '?'))
            __builtin_trap();
    }
}

void check_label(const std::uint8_t *data, std::size_t size)
{
    char text[av::ContextLabel::capacity * 2u + 2u] = {};
    const std::size_t kept = size < sizeof text - 1u ? size : sizeof text - 1u;
    for (std::size_t at = 0u; at < kept; at++) text[at] = data[at] == 0u ? 'x' : static_cast<char>(data[at]);
    const std::size_t source = std::strlen(text);
    const av::ContextLabel label(text);
    const std::size_t      length = std::strlen(label.text());
    if (length > av::ContextLabel::capacity || length != (source < av::ContextLabel::capacity ? source
                                                                                                : av::ContextLabel::capacity))
        __builtin_trap();
    if (std::memcmp(label.text(), text, length) != 0 || label.text()[av::ContextLabel::capacity] != '\0')
        __builtin_trap();
    const av::ContextLabel absent(nullptr);
    if (absent.text()[0] != '\0') __builtin_trap();
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    check_request(data, size);
    check_label(data, size);
    return 0;
}
