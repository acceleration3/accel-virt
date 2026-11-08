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
      output_(nullptr), source_width_(0u), source_height_(0u), width_(0u), height_(0u), format_(0u)
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

ID3D11Texture2D *HostFormatConverter::draw(ID3D11Device5 *device, ID3D11DeviceContext4 *context,
                                           ID3D11Texture2D *source, uint32_t subresource, const Rectangle &from,
                                           uint32_t width, uint32_t height, uint32_t target_format)
{
    AV_SCOPE("HostFormatConverter::draw");
    if (device == nullptr || context == nullptr || source == nullptr || width == 0u || height == 0u ||
        !prepare(device, context, source, width, height, target_format))
        return nullptr;
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
