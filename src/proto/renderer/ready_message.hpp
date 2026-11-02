#ifndef AV_READY_MESSAGE_HPP
#define AV_READY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ReadyMessage {
public:
    static constexpr uint32_t lanes = 8u;
    static constexpr size_t   encoded_size = 12u;
    static constexpr uint32_t descriptors = 0u;

    ReadyMessage();
    ReadyMessage(uint32_t version, uint32_t executor_abi, uint32_t sub_apis);

    uint32_t version() const;
    uint32_t executor_abi() const;
    uint32_t sub_apis() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t version_;
    uint32_t executor_abi_;
    uint32_t sub_apis_;
};

}

#endif
