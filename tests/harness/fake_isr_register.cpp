#include "fake_isr_register.hpp"

#include "test_registry.hpp"

#include <cstdint>

namespace av {

FakeIsrRegister::FakeIsrRegister() : status_(0u) {}

std::uint64_t FakeIsrRegister::length() const { return 0x1000u; }

void FakeIsrRegister::unexpected(const char *access)
{
    TestRegistry::instance().record_failure(__FILE__, __LINE__, "unexpected %s on ISR", access);
}

std::uint8_t FakeIsrRegister::read_u8(std::uint64_t offset)
{
    if (offset != 0u) { unexpected("read_u8 offset"); return 0u; }
    const std::uint8_t status = status_;
    status_ = 0u;
    return status;
}

std::uint16_t FakeIsrRegister::read_u16(std::uint64_t) { unexpected("read_u16"); return 0u; }
std::uint32_t FakeIsrRegister::read_u32(std::uint64_t) { unexpected("read_u32"); return 0u; }
void FakeIsrRegister::write_u8(std::uint64_t, std::uint8_t)   { unexpected("write_u8"); }
void FakeIsrRegister::write_u16(std::uint64_t, std::uint16_t) { unexpected("write_u16"); }
void FakeIsrRegister::write_u32(std::uint64_t, std::uint32_t) { unexpected("write_u32"); }
void FakeIsrRegister::write_u64(std::uint64_t, std::uint64_t) { unexpected("write_u64"); }

void FakeIsrRegister::raise(std::uint8_t bits) { status_ = static_cast<std::uint8_t>(status_ | bits); }

}  // namespace av
