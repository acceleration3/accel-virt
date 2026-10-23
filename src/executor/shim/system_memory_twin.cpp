#include "executor/shim/system_memory_twin.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/dxvk_vulkan_device.hpp"
#include "executor/shim/exported_image_layout.hpp"
#include "proto/vulkan/exportable_image_info.hpp"

namespace av {

namespace {

/* DRM_FORMAT_MOD_LINEAR, from drm_fourcc.h. */
constexpr uint64_t drm_format_mod_linear = 0u;

}  // namespace

SystemMemoryTwin::SystemMemoryTwin(ExportableImages &owner)
    : TextureHold(owner), image_(), texture_(nullptr), width_(0u), height_(0u), format_(0u), copied_writes_(0u),
      copied_(false)
{
    AV_SCOPE("SystemMemoryTwin::SystemMemoryTwin");
}

SystemMemoryTwin::~SystemMemoryTwin()
{
    AV_SCOPE("SystemMemoryTwin::~SystemMemoryTwin");
    free();
}

/* Linear under the one modifier every driver reads alike, as the display's
 * image is (HostImageExport), and in system memory only: a twin in VRAM would
 * be read across the bus all the same, but VRAM that is not host visible is
 * no memory another GPU can open, so a GPU with no such type has no twin. */
bool SystemMemoryTwin::make(DxvkVulkanDevice &vulkan, ID3D11Texture2D *source)
{
    AV_SCOPE("SystemMemoryTwin::make");
    if (made() || !vulkan.connected()) return false;
    D3D11_TEXTURE2D_DESC found = {};
    source->GetDesc(&found);
    const ExportableImageInfo described(found.Width, found.Height, 1u, 1u, 1u, static_cast<uint32_t>(found.Format),
                                        found.BindFlags);
    if (!described.supported() || !vulkan.table().supports_exportable(described.info())) return false;
    D3D11_TEXTURE2D_DESC1 wrapped = {};
    wrapped.Width = found.Width;
    wrapped.Height = found.Height;
    wrapped.MipLevels = 1u;
    wrapped.ArraySize = 1u;
    wrapped.Format = found.Format;
    wrapped.SampleDesc = {1u, 0u};
    wrapped.Usage = D3D11_USAGE_DEFAULT;
    wrapped.BindFlags = found.BindFlags;
    if (!image_.create(vulkan.table(), described.info())) return false;
    const uint32_t type = vulkan.table().system_memory_type(image_.requirements().memoryTypeBits);
    if (type == VK_MAX_MEMORY_TYPES || !image_.bind_exportable(type) || !vulkan.wrap(wrapped, image_.image(), texture_)) {
        free();
        return false;
    }
    width_ = found.Width;
    height_ = found.Height;
    format_ = static_cast<uint32_t>(found.Format);
    return true;
}

bool SystemMemoryTwin::made() const
{
    AV_SCOPE("SystemMemoryTwin::made");
    return texture_ != nullptr;
}

void SystemMemoryTwin::copy_from(ID3D11DeviceContext4 *context, ID3D11Texture2D *source, uint64_t writes)
{
    AV_SCOPE("SystemMemoryTwin::copy_from");
    if (copied_ && copied_writes_ == writes) return;
    copied_ = true;
    copied_writes_ = writes;
    context->CopySubresourceRegion(texture_, 0u, 0u, 0u, 0u, source, 0u, nullptr);
    context->Flush();
}

bool SystemMemoryTwin::describe(AvExportedImage &described) const
{
    AV_SCOPE("SystemMemoryTwin::describe");
    described = AvExportedImage{-1, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    OwnedFd memory;
    if (!made() || !image_.export_dma_buf(memory) ||
        !ExportedImageLayout(image_, width_, height_, format_, drm_format_mod_linear).describe(memory.fd(), described))
        return false;
    static_cast<void>(memory.take());
    return true;
}

void SystemMemoryTwin::free()
{
    AV_SCOPE("SystemMemoryTwin::free");
    if (texture_ != nullptr) texture_->Release();
    texture_ = nullptr;
    image_.release();
}

}  // namespace av
