#include "core/pci/mapped_register_window.hpp"

namespace av {

MappedRegisterWindow::MappedRegisterWindow(Platform &platform, void *base, uint64_t length)
    : platform_(platform), base_(static_cast<uint8_t *>(base)), length_(length) {}

void *MappedRegisterWindow::at(uint64_t offset) const { return base_ + offset; }

uint64_t MappedRegisterWindow::length() const { return length_; }

uint8_t MappedRegisterWindow::read_u8(uint64_t offset)
{
    platform_.require_irql_at_most(Irql::isr, "MappedRegisterWindow::read_u8");
    return platform_.read_register_u8(at(offset));
}

uint16_t MappedRegisterWindow::read_u16(uint64_t offset)
{
    platform_.require_irql_at_most(Irql::isr, "MappedRegisterWindow::read_u16");
    return platform_.read_register_u16(at(offset));
}

uint32_t MappedRegisterWindow::read_u32(uint64_t offset)
{
    platform_.require_irql_at_most(Irql::isr, "MappedRegisterWindow::read_u32");
    return platform_.read_register_u32(at(offset));
}

void MappedRegisterWindow::write_u8(uint64_t offset, uint8_t value)
{
    platform_.require_irql_at_most(Irql::isr, "MappedRegisterWindow::write_u8");
    platform_.write_register_u8(at(offset), value);
}

void MappedRegisterWindow::write_u16(uint64_t offset, uint16_t value)
{
    platform_.require_irql_at_most(Irql::isr, "MappedRegisterWindow::write_u16");
    platform_.write_register_u16(at(offset), value);
}

void MappedRegisterWindow::write_u32(uint64_t offset, uint32_t value)
{
    platform_.require_irql_at_most(Irql::isr, "MappedRegisterWindow::write_u32");
    platform_.write_register_u32(at(offset), value);
}

void MappedRegisterWindow::write_u64(uint64_t offset, uint64_t value)
{
    platform_.require_irql_at_most(Irql::isr, "MappedRegisterWindow::write_u64");
    platform_.write_register_u64(at(offset), value);
}

}  // namespace av
