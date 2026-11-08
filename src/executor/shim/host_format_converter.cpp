#include "executor/shim/host_format_converter.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {

template <typename Interface> void drop(Interface *&object)
{
    AV_SCOPE("drop");
    if (object != nullptr) object->Release();
    object = nullptr;
}

}

HostFormatConverter::HostFormatConverter()
    : video_(nullptr), video_context_(nullptr), enumerator_(nullptr), processor_(nullptr), target_(nullptr),
      output_(nullptr), source_width_(0u), source_height_(0u), width_(0u), height_(0u), format_(0u), rows_(), read_(nullptr),
      written_(nullptr), read_width_(0u), read_height_(0u), read_format_(0u), written_width_(0u), written_height_(0u),
      written_format_(0u)
{
    AV_SCOPE("HostFormatConverter::HostFormatConverter");
}

HostFormatConverter::~HostFormatConverter()
{
    AV_SCOPE("HostFormatConverter::~HostFormatConverter");
    release();
}

void HostFormatConverter::release()
{
    AV_SCOPE("HostFormatConverter::release");
    drop(read_);
    drop(written_);
    read_width_ = read_height_ = read_format_ = 0u;
    written_width_ = written_height_ = written_format_ = 0u;
    drop(output_);
    drop(target_);
    drop(processor_);
    drop(enumerator_);
    drop(video_context_);
    drop(video_);
    source_width_ = 0u;
    source_height_ = 0u;
    width_ = 0u;
    height_ = 0u;
    format_ = 0u;
}

bool HostFormatConverter::prepare(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Texture2D *source,
                                  uint32_t width, uint32_t height, uint32_t target_format)
{
    AV_SCOPE("HostFormatConverter::prepare");
    D3D11_TEXTURE2D_DESC from = {};
    source->GetDesc(&from);
    if (output_ != nullptr && from.Width == source_width_ && from.Height == source_height_ && width == width_ &&
        height == height_ && target_format == format_)
        return true;
    release();
    if (FAILED(device->QueryInterface(__uuidof(ID3D11VideoDevice), reinterpret_cast<void **>(&video_))) ||
        FAILED(context->QueryInterface(__uuidof(ID3D11VideoContext), reinterpret_cast<void **>(&video_context_))))
        return false;
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content = {};
    content.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content.InputWidth = from.Width;
    content.InputHeight = from.Height;
    content.OutputWidth = width;
    content.OutputHeight = height;
    content.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
    if (FAILED(video_->CreateVideoProcessorEnumerator(&content, &enumerator_)) ||
        FAILED(video_->CreateVideoProcessor(enumerator_, 0u, &processor_)))
        return false;
    D3D11_TEXTURE2D_DESC described = {};
    described.Width = width;
    described.Height = height;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = static_cast<DXGI_FORMAT>(target_format);
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_DEFAULT;
    described.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output = {};
    output.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    if (FAILED(device->CreateTexture2D(&described, nullptr, &target_)) ||
        FAILED(video_->CreateVideoProcessorOutputView(target_, enumerator_, &output, &output_)))
        return false;
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE rgb = {};
    video_context_->VideoProcessorSetOutputColorSpace(processor_, &rgb);
    source_width_ = from.Width;
    source_height_ = from.Height;
    width_ = width;
    height_ = height;
    format_ = target_format;
    return true;
}

bool HostFormatConverter::stage(ID3D11Device5 *device, ID3D11Texture2D *&texture, uint32_t &held_width,
                                uint32_t &held_height, uint32_t &held_format, uint32_t width, uint32_t height,
                                uint32_t format, bool writing)
{
    AV_SCOPE("HostFormatConverter::stage");
    if (texture != nullptr && held_width == width && held_height == height && held_format == format) return true;
    drop(texture);
    D3D11_TEXTURE2D_DESC described = {};
    described.Width = width;
    described.Height = height;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = static_cast<DXGI_FORMAT>(format);
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_STAGING;
    described.CPUAccessFlags = writing ? D3D11_CPU_ACCESS_WRITE : D3D11_CPU_ACCESS_READ;
    if (FAILED(device->CreateTexture2D(&described, nullptr, &texture))) {
        texture = nullptr;
        return false;
    }
    held_width = width;
    held_height = height;
    held_format = format;
    return true;
}

ID3D11Texture2D *HostFormatConverter::draw_scrgb(ID3D11Device5 *device, ID3D11DeviceContext4 *context,
                                                 ID3D11Texture2D *source, uint32_t subresource,
                                                 const Rectangle &from, uint32_t width, uint32_t height,
                                                 uint32_t target_format)
{
    AV_SCOPE("HostFormatConverter::draw_scrgb");
    D3D11_TEXTURE2D_DESC origin = {};
    source->GetDesc(&origin);
    const uint32_t right = from.right < origin.Width ? from.right : origin.Width;
    const uint32_t bottom = from.bottom < origin.Height ? from.bottom : origin.Height;
    if (right <= from.left || bottom <= from.top) return nullptr;
    const uint32_t across = right - from.left;
    const uint32_t down = bottom - from.top;
    if (!stage(device, read_, read_width_, read_height_, read_format_, across, down, origin.Format, false) ||
        !stage(device, written_, written_width_, written_height_, written_format_, width, height, target_format, true))
        return nullptr;
    const D3D11_BOX region = {from.left, from.top, 0u, right, bottom, 1u};
    context->CopySubresourceRegion(read_, 0u, 0u, 0u, 0u, source, subresource, &region);
    D3D11_MAPPED_SUBRESOURCE in = {};
    if (FAILED(context->Map(read_, 0u, D3D11_MAP_READ, 0u, &in))) return nullptr;
    D3D11_MAPPED_SUBRESOURCE out = {};
    if (FAILED(context->Map(written_, 0u, D3D11_MAP_WRITE, 0u, &out))) {
        context->Unmap(read_, 0u);
        return nullptr;
    }
    const uint32_t stride = rows_.pixel_bytes();
    for (uint32_t row = 0u; row < height; row++) {
        const uint8_t *const line = static_cast<const uint8_t *>(in.pData) +
                                    static_cast<size_t>(row * down / height) * in.RowPitch;
        uint8_t *const to = static_cast<uint8_t *>(out.pData) + static_cast<size_t>(row) * out.RowPitch;
        if (across == width) {
            rows_.convert(line, width, to);
            continue;
        }
        for (uint32_t column = 0u; column < width; column++)
            rows_.convert(line + static_cast<size_t>(column * across / width) * stride, 1u, to + column * 4u);
    }
    context->Unmap(written_, 0u);
    context->Unmap(read_, 0u);
    return written_;
}

ID3D11Texture2D *HostFormatConverter::draw(ID3D11Device5 *device, ID3D11DeviceContext4 *context,
                                           ID3D11Texture2D *source, uint32_t subresource, const Rectangle &from,
                                           uint32_t width, uint32_t height, uint32_t target_format)
{
    AV_SCOPE("HostFormatConverter::draw");
    if (device == nullptr || context == nullptr || source == nullptr || width == 0u || height == 0u) return nullptr;
    D3D11_TEXTURE2D_DESC origin = {};
    source->GetDesc(&origin);
    if (rows_.use(origin.Format, target_format))
        return draw_scrgb(device, context, source, subresource, from, width, height, target_format);
    if (!prepare(device, context, source, width, height, target_format)) return nullptr;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input = {};
    input.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    input.Texture2D.ArraySlice = subresource;
    ID3D11VideoProcessorInputView *view = nullptr;
    if (FAILED(video_->CreateVideoProcessorInputView(source, enumerator_, &input, &view))) return nullptr;
    const RECT source_rect = {static_cast<LONG>(from.left), static_cast<LONG>(from.top), static_cast<LONG>(from.right),
                              static_cast<LONG>(from.bottom)};
    const RECT target_rect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    video_context_->VideoProcessorSetStreamFrameFormat(processor_, 0u, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    video_context_->VideoProcessorSetStreamSourceRect(processor_, 0u, TRUE, &source_rect);
    video_context_->VideoProcessorSetStreamDestRect(processor_, 0u, TRUE, &target_rect);
    video_context_->VideoProcessorSetStreamAutoProcessingMode(processor_, 0u, FALSE);
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE rgb = {};
    video_context_->VideoProcessorSetStreamColorSpace(processor_, 0u, &rgb);
    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = TRUE;
    stream.pInputSurface = view;
    const bool drawn = SUCCEEDED(video_context_->VideoProcessorBlt(processor_, output_, 0u, 1u, &stream));
    view->Release();
    return drawn ? target_ : nullptr;
}

}
