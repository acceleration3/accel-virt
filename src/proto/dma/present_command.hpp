#ifndef AV_PRESENT_COMMAND_HPP
#define AV_PRESENT_COMMAND_HPP

#include "proto/dma/present_address_space.hpp"
#include "proto/dma/present_operation.hpp"
#include "proto/dma/rectangle.hpp"

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
                   uint32_t flip_interval, uint32_t sub_rectangles, PresentAddressSpace addresses);

    void write(WireWriter &writer) const;

private:
    static constexpr uint32_t magic = 0x00505641u;

    PresentOperation operation_;
    uint32_t         color_;
    Rectangle        source_;
    Rectangle        destination_;
    uint32_t         flip_interval_;
    uint32_t         sub_rectangles_;
    PresentAddressSpace addresses_;
};

}

#endif
