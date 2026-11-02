#ifndef AV_FAKE_REGISTER_BYTES_HPP
#define AV_FAKE_REGISTER_BYTES_HPP

#include "core/platform/register_window.hpp"

#include <cstdint>

namespace av {

class FakeRegisterBytes final : public RegisterWindow {
public:
    static constexpr std::uint32_t capacity = 64u;

    explicit FakeRegisterBytes(std::uint64_t length);

    std::uint64_t length() const override;
    std::uint8_t  read_u8(std::uint64_t offset) override;
    std::uint16_t read_u16(std::uint64_t offset) override;
    std::uint32_t read_u32(std::uint64_t offset) override;
    void          write_u8(std::uint64_t offset, std::uint8_t value) override;
    void          write_u16(std::uint64_t offset, std::uint16_t value) override;
    void          write_u32(std::uint64_t offset, std::uint32_t value) override;
    void          write_u64(std::uint64_t offset, std::uint64_t value) override;

    void          set_u32(std::uint64_t offset, std::uint32_t value);
    std::uint32_t writes() const;
    std::uint64_t last_write_offset() const;
    std::uint64_t last_write_value() const;

private:
    std::uint64_t read(std::uint64_t offset, unsigned width);
    void          write(std::uint64_t offset, std::uint64_t value, unsigned width);

    std::uint8_t  bytes_[capacity];
    std::uint64_t length_;
    std::uint32_t writes_;
    std::uint64_t last_write_offset_;
    std::uint64_t last_write_value_;
};

}

#endif
