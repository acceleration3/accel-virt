#ifndef AV_FAKE_DOORBELL_PAGES_HPP
#define AV_FAKE_DOORBELL_PAGES_HPP

#include "core/platform/register_window.hpp"
#include "proto/context/context_capacity.hpp"

#include <cstdint>

namespace av {

class FakeDoorbellPages final : public RegisterWindow {
public:
    static constexpr std::uint32_t pages = ContextCapacity::contexts;
    static constexpr std::uint64_t page_bytes = 4096u;

    FakeDoorbellPages();

    std::uint64_t length() const override;
    std::uint8_t  read_u8(std::uint64_t offset) override;
    std::uint16_t read_u16(std::uint64_t offset) override;
    std::uint32_t read_u32(std::uint64_t offset) override;
    void          write_u8(std::uint64_t offset, std::uint8_t value) override;
    void          write_u16(std::uint64_t offset, std::uint16_t value) override;
    void          write_u32(std::uint64_t offset, std::uint32_t value) override;
    void          write_u64(std::uint64_t offset, std::uint64_t value) override;

    std::uint32_t rings(std::uint32_t page) const;
    std::uint32_t stray_stores() const;

private:
    void store(std::uint64_t offset, unsigned width);

    std::uint32_t rings_[pages];
    std::uint32_t stray_;
};

}

#endif
