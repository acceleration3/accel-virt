#include "mmio_register_window.hpp"

namespace av {

MmioRegisterWindow::MmioRegisterWindow(PUCHAR base, uint64_t length) : base_(base), length_(length) {}

uint64_t MmioRegisterWindow::length() const { return length_; }

uint8_t MmioRegisterWindow::read_u8(uint64_t offset) { return READ_REGISTER_UCHAR(base_ + offset); }

uint16_t MmioRegisterWindow::read_u16(uint64_t offset)
{
    return READ_REGISTER_USHORT(reinterpret_cast<PUSHORT>(base_ + offset));
}

uint32_t MmioRegisterWindow::read_u32(uint64_t offset)
{
    return READ_REGISTER_ULONG(reinterpret_cast<PULONG>(base_ + offset));
}

void MmioRegisterWindow::write_u8(uint64_t offset, uint8_t value)
{
    WRITE_REGISTER_UCHAR(base_ + offset, value);
}

void MmioRegisterWindow::write_u16(uint64_t offset, uint16_t value)
{
    WRITE_REGISTER_USHORT(reinterpret_cast<PUSHORT>(base_ + offset), value);
}

void MmioRegisterWindow::write_u32(uint64_t offset, uint32_t value)
{
    WRITE_REGISTER_ULONG(reinterpret_cast<PULONG>(base_ + offset), value);
}

void MmioRegisterWindow::write_u64(uint64_t offset, uint64_t value)
{
    WRITE_REGISTER_ULONG64(reinterpret_cast<PULONG64>(base_ + offset), value);
}

}  // namespace av
