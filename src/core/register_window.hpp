/* One mapped register region of a device - a BAR range holding a virtio
 * structure. Two implementations exist: MmioRegisterWindow (src/km) over a
 * mapped BAR, and the fake device's regions (tests/harness). Callers stay
 * within length(); the implementation does not check. */
#ifndef AV_REGISTER_WINDOW_HPP
#define AV_REGISTER_WINDOW_HPP

#include <stdint.h>

namespace av {

class RegisterWindow {
public:
    RegisterWindow() = default;
    RegisterWindow(const RegisterWindow &) = delete;
    RegisterWindow &operator=(const RegisterWindow &) = delete;

    virtual uint64_t length() const = 0;

    virtual uint8_t  read_u8(uint64_t offset) = 0;
    virtual uint16_t read_u16(uint64_t offset) = 0;
    virtual uint32_t read_u32(uint64_t offset) = 0;
    virtual void     write_u8(uint64_t offset, uint8_t value) = 0;
    virtual void     write_u16(uint64_t offset, uint16_t value) = 0;
    virtual void     write_u32(uint64_t offset, uint32_t value) = 0;
    virtual void     write_u64(uint64_t offset, uint64_t value) = 0;

protected:
    ~RegisterWindow() = default;
};

}  // namespace av

#endif  // AV_REGISTER_WINDOW_HPP
