#include "proto/export/exported_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ExportedLayout::ExportedLayout() : width_(0u), height_(0u), fourcc_(0u), stride_(0u), offset_(0u), modifier_(0u)
{
    AV_SCOPE("ExportedLayout::ExportedLayout");
}

ExportedLayout::ExportedLayout(uint32_t width, uint32_t height, uint32_t fourcc, uint32_t stride, uint32_t offset,
                               uint64_t modifier)
    : width_(width), height_(height), fourcc_(fourcc), stride_(stride), offset_(offset), modifier_(modifier)
{
    AV_SCOPE("ExportedLayout::ExportedLayout");
}

uint32_t ExportedLayout::width() const
{
    AV_SCOPE("ExportedLayout::width");
    return width_;
}

uint32_t ExportedLayout::height() const
{
    AV_SCOPE("ExportedLayout::height");
    return height_;
}

uint32_t ExportedLayout::fourcc() const
{
    AV_SCOPE("ExportedLayout::fourcc");
    return fourcc_;
}

uint32_t ExportedLayout::stride() const
{
    AV_SCOPE("ExportedLayout::stride");
    return stride_;
}

uint32_t ExportedLayout::offset() const
{
    AV_SCOPE("ExportedLayout::offset");
    return offset_;
}

uint64_t ExportedLayout::modifier() const
{
    AV_SCOPE("ExportedLayout::modifier");
    return modifier_;
}

bool ExportedLayout::describes_a_picture() const
{
    AV_SCOPE("ExportedLayout::describes_a_picture");
    return width_ != 0u && height_ != 0u && fourcc_ != 0u && stride_ >= width_;
}

bool ExportedLayout::empty() const
{
    AV_SCOPE("ExportedLayout::empty");
    return width_ == 0u && height_ == 0u && fourcc_ == 0u && stride_ == 0u && offset_ == 0u && modifier_ == 0u;
}

}
