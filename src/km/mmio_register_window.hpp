/* A register region inside a BAR mapped with DxgkCbMapMemory. Translation
 * only: every member is one READ_REGISTER_* or WRITE_REGISTER_* call; bounds
 * are the caller's, as RegisterWindow states. */
#ifndef AV_MMIO_REGISTER_WINDOW_HPP
#define AV_MMIO_REGISTER_WINDOW_HPP

#include <ntddk.h>

#include "register_window.hpp"

namespace av {

class MmioRegisterWindow final : public RegisterWindow {
public:
    MmioRegisterWindow(PUCHAR base, uint64_t length);

    uint64_t length() const override;
    uint8_t  read_u8(uint64_t offset) override;
    uint16_t read_u16(uint64_t offset) override;
    uint32_t read_u32(uint64_t offset) override;
    void     write_u8(uint64_t offset, uint8_t value) override;
    void     write_u16(uint64_t offset, uint16_t value) override;
    void     write_u32(uint64_t offset, uint32_t value) override;
    void     write_u64(uint64_t offset, uint64_t value) override;

private:
    PUCHAR   base_;
    uint64_t length_;
};

}  // namespace av

#endif  // AV_MMIO_REGISTER_WINDOW_HPP
