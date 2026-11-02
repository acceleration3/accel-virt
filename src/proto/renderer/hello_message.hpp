#ifndef AV_HELLO_MESSAGE_HPP
#define AV_HELLO_MESSAGE_HPP

#include "core/support/status.hpp"
#include "proto/context/device_uuid.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class HelloMessage {
public:
    static constexpr size_t   fixed_size = 32u;
    static constexpr size_t   uuid_bytes = DeviceUuid::bytes;
    static constexpr uint32_t max_path_length = 4095u;
    static constexpr uint32_t max_capset_length = 4096u;
    static constexpr uint32_t descriptors = 0u;
    static constexpr uint32_t protocol_version = 1u;

    HelloMessage();
    HelloMessage(uint32_t version, uint32_t epoch, const DeviceUuid &gpu_uuid,
                 const char *executor_path, uint32_t path_length, const uint8_t *capset,
                 uint32_t capset_length);

    uint32_t       version() const;
    uint32_t       epoch() const;
    const uint8_t *gpu_uuid() const;
    const char    *executor_path() const;
    uint32_t       executor_path_length() const;
    const uint8_t *capset() const;
    uint32_t       capset_length() const;
    size_t         encoded_length() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t       version_;
    uint32_t       epoch_;
    DeviceUuid     gpu_uuid_;
    const char    *executor_path_;
    uint32_t       path_length_;
    const uint8_t *capset_;
    uint32_t       capset_length_;
};

}

#endif
