#ifndef AV_ICD_HANDED_FRAME_HPP
#define AV_ICD_HANDED_FRAME_HPP

#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av::icd {

template <typename Source, typename Mark>
class HandedFrame {
public:
    enum class Rows : uint8_t { bgra_bottom_up, bgra_top_down, rgba_top_down };

    HandedFrame();
    HandedFrame(Source &owner, uint32_t slot, const uint8_t *pixels, Rows rows, uint32_t width, uint32_t height,
                void *window, const Mark &mark);

    Source        *owner() const;
    uint32_t       slot() const;
    const uint8_t *pixels() const;
    Rows           rows() const;
    bool           chained() const;
    uint32_t       width() const;
    uint32_t       height() const;
    void          *window() const;
    const Mark    &mark() const;

private:
    Source        *owner_;
    uint32_t       slot_;
    const uint8_t *pixels_;
    Rows           rows_;
    uint32_t       width_;
    uint32_t       height_;
    void          *window_;
    Mark           mark_;
};

template <typename Source, typename Mark>
HandedFrame<Source, Mark>::HandedFrame()
    : owner_(nullptr), slot_(0u), pixels_(nullptr), rows_(Rows::bgra_bottom_up), width_(0u), height_(0u),
      window_(nullptr), mark_()
{
    AV_SCOPE("HandedFrame::HandedFrame");
}

template <typename Source, typename Mark>
HandedFrame<Source, Mark>::HandedFrame(Source &owner, uint32_t slot, const uint8_t *pixels, Rows rows,
                                       uint32_t width, uint32_t height, void *window, const Mark &mark)
    : owner_(&owner), slot_(slot), pixels_(pixels), rows_(rows), width_(width), height_(height), window_(window),
      mark_(mark)
{
    AV_SCOPE("HandedFrame::HandedFrame");
}

template <typename Source, typename Mark>
Source *HandedFrame<Source, Mark>::owner() const
{
    AV_SCOPE("HandedFrame::owner");
    return owner_;
}

template <typename Source, typename Mark>
uint32_t HandedFrame<Source, Mark>::slot() const
{
    AV_SCOPE("HandedFrame::slot");
    return slot_;
}

template <typename Source, typename Mark>
const uint8_t *HandedFrame<Source, Mark>::pixels() const
{
    AV_SCOPE("HandedFrame::pixels");
    return pixels_;
}

template <typename Source, typename Mark>
typename HandedFrame<Source, Mark>::Rows HandedFrame<Source, Mark>::rows() const
{
    AV_SCOPE("HandedFrame::rows");
    return rows_;
}

template <typename Source, typename Mark>
bool HandedFrame<Source, Mark>::chained() const
{
    AV_SCOPE("HandedFrame::chained");
    return owner_ != nullptr && pixels_ == nullptr;
}

template <typename Source, typename Mark>
uint32_t HandedFrame<Source, Mark>::width() const
{
    AV_SCOPE("HandedFrame::width");
    return width_;
}

template <typename Source, typename Mark>
uint32_t HandedFrame<Source, Mark>::height() const
{
    AV_SCOPE("HandedFrame::height");
    return height_;
}

template <typename Source, typename Mark>
void *HandedFrame<Source, Mark>::window() const
{
    AV_SCOPE("HandedFrame::window");
    return window_;
}

template <typename Source, typename Mark>
const Mark &HandedFrame<Source, Mark>::mark() const
{
    AV_SCOPE("HandedFrame::mark");
    return mark_;
}

}

#endif
