#include "icd/d3d/texture_upload.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/row_pieces.hpp"
#include "icd/d3d/upload_sink.hpp"
#include "proto/resources/pitched_extent.hpp"

namespace av::d3d {

TextureUpload::TextureUpload(const TexelLayout &layout, const d3dproto::Box &box, uint32_t row_pitch,
                             uint32_t slice_pitch, uint64_t most)
    : layout_(layout), box_(box), row_pitch_(row_pitch), slice_pitch_(slice_pitch), most_(most)
{
    AV_SCOPE("TextureUpload::TextureUpload");
}

uint32_t TextureUpload::width() const
{
    AV_SCOPE("TextureUpload::width");
    return box_.right > box_.left ? box_.right - box_.left : 0u;
}

uint32_t TextureUpload::height() const
{
    AV_SCOPE("TextureUpload::height");
    return box_.bottom > box_.top ? box_.bottom - box_.top : 0u;
}

uint32_t TextureUpload::slices() const
{
    AV_SCOPE("TextureUpload::slices");
    return box_.back > box_.front ? box_.back - box_.front : 0u;
}

PitchedExtent TextureUpload::extent() const
{
    AV_SCOPE("TextureUpload::extent");
    return PitchedExtent(layout_.row_bytes(width()), layout_.rows(height()), slices(), row_pitch_, slice_pitch_);
}

uint64_t TextureUpload::bytes() const
{
    AV_SCOPE("TextureUpload::bytes");
    uint64_t reach = 0u;
    return extent().end(0u, reach) ? reach : 0u;
}

bool TextureUpload::readable() const
{
    AV_SCOPE("TextureUpload::readable");
    return extent().pitches_hold();
}

bool TextureUpload::send_slice(UploadSink &sink, const uint8_t *data, uint32_t slice) const
{
    AV_SCOPE("TextureUpload::send_slice");
    const uint8_t *const plane = data + uint64_t{slice} * slice_pitch_;
    icd::RowPieces       pieces(layout_, width(), height(), row_pitch_, most_);
    while (pieces.next()) {
        const d3dproto::Box part = {box_.left + pieces.left(),  box_.top + pieces.top(),       box_.front + slice,
                                    box_.left + pieces.right(), box_.top + pieces.bottom(), box_.front + slice + 1u};
        if (!sink.piece(part, plane + pieces.offset(), pieces.bytes(), row_pitch_, slice_pitch_,
                        slice == 0u && pieces.first()))
            return false;
    }
    return true;
}

bool TextureUpload::send(UploadSink &sink, const uint8_t *data) const
{
    AV_SCOPE("TextureUpload::send");
    if (!readable()) return false;
    if (bytes() <= most_) return sink.piece(box_, data, bytes(), row_pitch_, slice_pitch_, true);
    for (uint32_t slice = 0u; slice < slices(); slice++)
        if (!send_slice(sink, data, slice)) return false;
    return true;
}

}
