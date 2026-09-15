/* The commands a transport's last submission carried, read back off its ring in
 * MockKmt the way the device reads a batch: the submission descriptor names
 * where the batch starts, its header says how long it is, and the commands must
 * frame it exactly. How a test sees every command a batch shipped, where
 * MockKmt's journal keeps only each submission's first. */
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
    /* Room for the largest batch a connection ships. */
    static constexpr std::size_t max_bytes = 256u * 1024u;

    /* `ring` is the ring allocation's handle. */
    ShippedBatch(MockKmt &kmt, std::uint32_t ring);
    ~ShippedBatch();
    ShippedBatch(const ShippedBatch &) = delete;
    ShippedBatch &operator=(const ShippedBatch &) = delete;

    /* Copies the last submission's batch: malformed when the ring, the
     * descriptor or the batch header does not decode, or the commands do not
     * frame the batch exactly. */
    [[nodiscard]] Status read();
    std::uint32_t        commands() const;
    /* The command at `index` in shipping order: its header, and its body. */
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

}  // namespace av

#endif  // AV_SHIPPED_BATCH_HPP
