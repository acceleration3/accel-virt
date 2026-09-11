/* The doorbell BAR as the guest core writes it in the loopback suite: a 4-byte
 * store at offset 0 of a page rings that page on the device's MockHost, as a
 * KVM ioeventfd would; any other store does nothing. */
#ifndef AV_LOOPBACK_DOORBELLS_HPP
#define AV_LOOPBACK_DOORBELLS_HPP

#include "core/platform/register_window.hpp"

#include <cstdint>

namespace av {

class MockHost;

class LoopbackDoorbells final : public RegisterWindow {
public:
    static constexpr std::uint32_t pages = 64u;
    static constexpr std::uint64_t page_bytes = 4096u;

    explicit LoopbackDoorbells(MockHost &host);

    std::uint64_t length() const override;
    std::uint8_t  read_u8(std::uint64_t offset) override;
    std::uint16_t read_u16(std::uint64_t offset) override;
    std::uint32_t read_u32(std::uint64_t offset) override;
    void          write_u8(std::uint64_t offset, std::uint8_t value) override;
    void          write_u16(std::uint64_t offset, std::uint16_t value) override;
    void          write_u32(std::uint64_t offset, std::uint32_t value) override;
    void          write_u64(std::uint64_t offset, std::uint64_t value) override;

    std::uint32_t stores() const;

private:
    MockHost     &host_;
    std::uint32_t stores_;
};

}  // namespace av

#endif  // AV_LOOPBACK_DOORBELLS_HPP
