/* One register region inside a BAR dxgkrnl mapped: every access is one
 * Platform register read or write at the region's base plus the offset. */
#ifndef AV_MAPPED_REGISTER_WINDOW_HPP
#define AV_MAPPED_REGISTER_WINDOW_HPP

#include "core/platform/platform.hpp"
#include "core/platform/register_window.hpp"

#include <stdint.h>

namespace av {

class MappedRegisterWindow final : public RegisterWindow {
public:
    MappedRegisterWindow(Platform &platform, void *base, uint64_t length);

    uint64_t length() const override;
    uint8_t  read_u8(uint64_t offset) override;
    uint16_t read_u16(uint64_t offset) override;
    uint32_t read_u32(uint64_t offset) override;
    void     write_u8(uint64_t offset, uint8_t value) override;
    void     write_u16(uint64_t offset, uint16_t value) override;
    void     write_u32(uint64_t offset, uint32_t value) override;
    void     write_u64(uint64_t offset, uint64_t value) override;

    /* Where `offset` is in this mapping. What DxgkDdiConnectDoorbell answers
     * for a doorbell page (feature 049): dxgkrnl maps the pages behind that
     * address into the process that asked. Only a real mapping has one, which
     * is why it is here and not on RegisterWindow. */
    void    *address_at(uint64_t offset) const;

private:
    void *at(uint64_t offset) const;

    Platform &platform_;
    uint8_t  *base_;
    uint64_t  length_;
};

}  // namespace av

#endif  // AV_MAPPED_REGISTER_WINDOW_HPP
