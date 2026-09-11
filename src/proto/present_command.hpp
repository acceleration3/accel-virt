/* The header of one present in a DMA buffer (docs/protocol.md), written by
 * DxgkDdiPresent and executed by the device. Its sub-rectangles follow it; the
 * source and destination addresses are left zero for DxgkDdiPatch to fill. */
#ifndef AV_PRESENT_COMMAND_HPP
#define AV_PRESENT_COMMAND_HPP

#include "present_operation.hpp"
#include "rectangle.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireWriter;

class PresentCommand {
public:
    static constexpr size_t   header_size = 72u;
    static constexpr size_t   sub_rectangle_size = 16u;
    static constexpr uint32_t source_address_offset = 48u;
    static constexpr uint32_t destination_address_offset = 56u;

    PresentCommand(PresentOperation operation, uint32_t color, const Rectangle &source, const Rectangle &destination,
                   uint32_t flip_interval, uint32_t sub_rectangles);

    /* The header only; the writer latches a header that does not fit. */
    void write(WireWriter &writer) const;

private:
    static constexpr uint32_t magic = 0x504B5652u;   /* "RVKP" */

    PresentOperation operation_;
    uint32_t         color_;
    Rectangle        source_;
    Rectangle        destination_;
    uint32_t         flip_interval_;
    uint32_t         sub_rectangles_;
};

}  // namespace av

#endif  // AV_PRESENT_COMMAND_HPP
