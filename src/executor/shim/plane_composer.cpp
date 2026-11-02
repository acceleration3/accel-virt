#include "executor/shim/plane_composer.hpp"

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

D3D11_VIDEO_PROCESSOR_COLOR_SPACE colour_space_of(uint32_t flags)
{
    AV_SCOPE("colour_space_of");
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE space = {};
    if ((flags & AV_PLANE_YCBCR) == 0u) return space;
    space.YCbCr_Matrix = (flags & AV_PLANE_BT709) != 0u ? 1u : 0u;
    space.Nominal_Range = (flags & AV_PLANE_FULL_RANGE) != 0u ? D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255
                                                               : D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
    return space;
}

RECT rect_of(const int32_t *edges)
{
    AV_SCOPE("rect_of");
    return RECT{edges[0], edges[1], edges[2], edges[3]};
}

}

PlaneComposer::PlaneComposer()
    : video_(nullptr), video_context_(nullptr), enumerator_(nullptr), processor_(nullptr), target_(nullptr),
      target_view_(nullptr), output_(nullptr), width_(0u), height_(0u)
{
    AV_SCOPE("PlaneComposer::PlaneComposer");
}

PlaneComposer::~PlaneComposer()
{
    AV_SCOPE("PlaneComposer::~PlaneComposer");
    release();
}

void PlaneComposer::release()
{
    AV_SCOPE("PlaneComposer::release");
    drop(output_);
    drop(target_view_);
    drop(target_);
    drop(processor_);
    drop(enumerator_);
    drop(video_context_);
    drop(video_);
    width_ = 0u;
    height_ = 0u;
}

bool PlaneComposer::prepare(ID3D11Device5 *device, ID3D11DeviceContext4 *context, uint32_t width, uint32_t height)
{
    AV_SCOPE("PlaneComposer::prepare");
    if (output_ != nullptr && width == width_ && height == height_) return true;
    release();
    if (FAILED(device->QueryInterface(__uuidof(ID3D11VideoDevice), reinterpret_cast<void **>(&video_))) ||
        FAILED(context->QueryInterface(__uuidof(ID3D11VideoContext), reinterpret_cast<void **>(&video_context_))))
        return false;
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content = {};
    content.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content.InputWidth = width;
    content.InputHeight = height;
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
    described.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_DEFAULT;
    described.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&described, nullptr, &target_)) ||
        FAILED(device->CreateRenderTargetView(target_, nullptr, &target_view_)))
        return false;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output = {};
    output.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    if (FAILED(video_->CreateVideoProcessorOutputView(target_, enumerator_, &output, &output_))) return false;
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE desktop = {};
    video_context_->VideoProcessorSetOutputColorSpace(processor_, &desktop);
    width_ = width;
    height_ = height;
    return true;
}

ID3D11Texture2D *PlaneComposer::compose(ID3D11Device5 *device, ID3D11DeviceContext4 *context,
                                        const AvScanoutPlane *planes, ID3D11Texture2D *const *textures, uint32_t count,
                                        uint32_t width, uint32_t height)
{
    AV_SCOPE("PlaneComposer::compose");
    if (count > max_planes || !prepare(device, context, width, height)) return nullptr;
    const float black[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    context->ClearRenderTargetView(target_view_, black);
    ID3D11VideoProcessorInputView *inputs[max_planes] = {};
    D3D11_VIDEO_PROCESSOR_STREAM   streams[max_planes] = {};
    bool                           readable = true;
    for (uint32_t index = 0u; index < count && readable; index++) {
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input = {};
        input.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        readable = SUCCEEDED(video_->CreateVideoProcessorInputView(textures[index], enumerator_, &input, &inputs[index]));
        if (!readable) break;
        const RECT source = rect_of(planes[index].source);
        const RECT destination = rect_of(planes[index].destination);
        video_context_->VideoProcessorSetStreamFrameFormat(processor_, index, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
        video_context_->VideoProcessorSetStreamSourceRect(processor_, index, TRUE, &source);
        video_context_->VideoProcessorSetStreamDestRect(processor_, index, TRUE, &destination);
        video_context_->VideoProcessorSetStreamAutoProcessingMode(processor_, index, FALSE);
        const D3D11_VIDEO_PROCESSOR_COLOR_SPACE space = colour_space_of(planes[index].flags);
        video_context_->VideoProcessorSetStreamColorSpace(processor_, index, &space);
        streams[index].Enable = TRUE;
        streams[index].pInputSurface = inputs[index];
    }
    const bool composed =
        readable && (count == 0u || SUCCEEDED(video_context_->VideoProcessorBlt(processor_, output_, 0u, count, streams)));
    for (ID3D11VideoProcessorInputView *&input : inputs) drop(input);
    return composed ? target_ : nullptr;
}

}
