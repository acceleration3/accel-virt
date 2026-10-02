#include "harness/virtio/fake_register_bytes.hpp"

#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace av {

FakeRegisterBytes::FakeRegisterBytes(std::uint64_t length)
    : bytes_{}, length_(length), writes_(0u), last_write_offset_(0u),
      last_write_value_(0u) {}

std::uint64_t FakeRegisterBytes::length() const { return length_; }

std::uint64_t FakeRegisterBytes::read(std::uint64_t offset, unsigned width)
{
    if (offset + width > length_) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "read past register region at 0x%llx",
                                                static_cast<unsigned long long>(offset));
        return 0u;
    }
    std::uint64_t value = 0u;
    for (unsigned i = 0u; i < width && offset + i < capacity; i++) {
        value |= std::uint64_t{bytes_[offset + i]} << (8u * i);
    }
    return value;
}

void FakeRegisterBytes::write(std::uint64_t offset, std::uint64_t value, unsigned width)
{
    if (offset + width > length_) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "write past register region at 0x%llx",
                                                static_cast<unsigned long long>(offset));
        return;
    }
    writes_++;
    last_write_offset_ = offset;
    last_write_value_ = value;
    for (unsigned i = 0u; i < width && offset + i < capacity; i++)
        bytes_[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}

std::uint8_t  FakeRegisterBytes::read_u8(std::uint64_t offset)  { return static_cast<std::uint8_t>(read(offset, 1u)); }
std::uint16_t FakeRegisterBytes::read_u16(std::uint64_t offset) { return static_cast<std::uint16_t>(read(offset, 2u)); }
std::uint32_t FakeRegisterBytes::read_u32(std::uint64_t offset) { return static_cast<std::uint32_t>(read(offset, 4u)); }
void FakeRegisterBytes::write_u8(std::uint64_t offset, std::uint8_t value)   { write(offset, value, 1u); }
void FakeRegisterBytes::write_u16(std::uint64_t offset, std::uint16_t value) { write(offset, value, 2u); }
void FakeRegisterBytes::write_u32(std::uint64_t offset, std::uint32_t value) { write(offset, value, 4u); }
void FakeRegisterBytes::write_u64(std::uint64_t offset, std::uint64_t value) { write(offset, value, 8u); }

void FakeRegisterBytes::set_u32(std::uint64_t offset, std::uint32_t value)
{
    for (unsigned i = 0u; i < 4u; i++) bytes_[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
}

void FakeRegisterBytes::set_u8(std::uint64_t offset, std::uint8_t value) { bytes_[offset] = value; }

std::uint32_t FakeRegisterBytes::writes() const            { return writes_; }
std::uint64_t FakeRegisterBytes::last_write_offset() const { return last_write_offset_; }
std::uint64_t FakeRegisterBytes::last_write_value() const  { return last_write_value_; }

}  // namespace av
