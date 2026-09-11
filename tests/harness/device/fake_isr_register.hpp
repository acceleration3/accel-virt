/* The ISR status byte of a fake virtio device: reading it clears it
 * (virtio 1.2 4.1.4.5). Any other access fails the test. */
#ifndef AV_FAKE_ISR_REGISTER_HPP
#define AV_FAKE_ISR_REGISTER_HPP

#include "core/platform/register_window.hpp"

#include <cstdint>

namespace av {

class FakeIsrRegister final : public RegisterWindow {
public:
    FakeIsrRegister();

    std::uint64_t length() const override;
    std::uint8_t  read_u8(std::uint64_t offset) override;
    std::uint16_t read_u16(std::uint64_t offset) override;
    std::uint32_t read_u32(std::uint64_t offset) override;
    void          write_u8(std::uint64_t offset, std::uint8_t value) override;
    void          write_u16(std::uint64_t offset, std::uint16_t value) override;
    void          write_u32(std::uint64_t offset, std::uint32_t value) override;
    void          write_u64(std::uint64_t offset, std::uint64_t value) override;

    void raise(std::uint8_t bits);

private:
    void unexpected(const char *access);

    std::uint8_t status_;
};

}  // namespace av

#endif  // AV_FAKE_ISR_REGISTER_HPP
