#ifndef AV_SHIPPED_BATCH_HPP
#define AV_SHIPPED_BATCH_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockKmt;

class ShippedBatch {
public:
    static constexpr std::size_t max_bytes = 256u * 1024u;

    ShippedBatch(MockKmt &kmt, std::uint32_t ring);
    ~ShippedBatch();
    ShippedBatch(const ShippedBatch &) = delete;
    ShippedBatch &operator=(const ShippedBatch &) = delete;

    [[nodiscard]] Status read();
    std::uint32_t        commands() const;
    const CommandHeader &header(std::uint32_t index) const;
    const std::uint8_t  *body(std::uint32_t index) const;

private:
    static constexpr std::uint32_t max_commands = 16384u;

    MockKmt       &kmt_;
    std::uint32_t  ring_;
    std::uint8_t  *bytes_;
    std::uint32_t  count_;
    CommandHeader *headers_;
    std::uint32_t *offsets_;
};

}

#endif
