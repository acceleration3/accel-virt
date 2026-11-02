#ifndef AV_HOST_WINDOW_RECORD_HPP
#define AV_HOST_WINDOW_RECORD_HPP

#include "core/support/status.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class HostWindowRecord {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x00575641u;

    enum class Operation : uint16_t { open = 1u, close = 2u, adopt = 3u };

    HostWindowRecord();
    HostWindowRecord(Operation operation, uint64_t bytes, uint32_t resource_id, HostMemoryRequest memory);
    HostWindowRecord(uint64_t address, uint64_t bytes, HostMemoryRequest memory);

    bool     opens() const;
    bool     adopts() const;
    uint32_t resource_id() const;
    uint64_t bytes() const;
    HostMemoryRequest memory() const;
    uint64_t address() const;
    HostWindowRecord answered(uint32_t resource_id, uint64_t bytes, uint64_t address) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status accept_answer(const uint8_t *bytes, size_t length);

private:
    static constexpr uint16_t version = 3u;

    bool is_request() const;
    [[nodiscard]] Status read(const uint8_t *bytes, size_t length);

    uint16_t operation_;
    uint32_t resource_id_;
    uint32_t memory_;
    uint64_t bytes_;
    uint64_t address_;
};

}

#endif
