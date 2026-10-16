#include "executor/shim/exportable_images.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/dxvk_vulkan_device.hpp"
#include "executor/shim/vkd3d_image_export.hpp"
#include "proto/d3d/dxgi_format.hpp"
#include "proto/vulkan/exportable_image_info.hpp"

#include <stdio.h>
#include <unistd.h>

namespace av {

namespace {

/* The private data a texture made here carries: its ExportableImage. */
constexpr GUID image_data = {0x5e1b7c42, 0x93d0, 0x4f6a, {0x8c, 0x2e, 0x71, 0x0b, 0xd4, 0x96, 0x3a, 0x58}};
/* From drm_fourcc.h. */
constexpr uint64_t drm_format_mod_linear = 0u;
constexpr uint64_t drm_format_mod_invalid = 0x00ffffffffffffffull;
/* More tilings than any driver lists for one format and layout here. */
constexpr uint32_t most_tilings = 16u;

}  // namespace

ExportableImages::ExportableImages(DxvkVulkanDevice &vulkan)
    : vulkan_(vulkan), heap_(), images_(heap_), retired_(0u), sink_(stderr), lines_(sink_, "virtio-gpu-av: d3d11: ")
{
    AV_SCOPE("ExportableImages::ExportableImages");
}

ExportableImages::~ExportableImages()
{
    AV_SCOPE("ExportableImages::~ExportableImages");
    release();
}

/* The image is made as vkd3d-proton made its own, so the two lay the memory
 * out alike; memory that is not what this image asks for is a layout they do
 * not agree on, and is refused rather than shown wrong. Each refusal says why
 * in the host log, the only place it is seen: the guest learns only that its
 * buffer could not be shown. */
ID3D11Texture2D *ExportableImages::import(Vkd3dImageExport &image, const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("ExportableImages::import");
    const auto refuse = [this](const char *why) { lines_.name("an image cannot be imported:", why); };
    collect();
    if (!image.whole() || !vulkan_.connected()) {
        refuse(!image.whole() ? "the export is part of an allocation" : "DXVK's Vulkan device is not reached");
        return nullptr;
    }
    VkImage made = VK_NULL_HANDLE;
    if (!vulkan_.table().create_image(image.image(), made)) {
        refuse("vkCreateImage refused vkd3d-proton's description");
        return nullptr;
    }
    const VkMemoryRequirements needs = vulkan_.table().requirements(made);
    if (needs.size > image.size() || (needs.memoryTypeBits & (1u << image.memory_type())) == 0u) {
        LogLine(lines_)
            .text("an image cannot be imported: it needs ")
            .number(needs.size)
            .text(" bytes of types ")
            .hex(needs.memoryTypeBits, 8u)
            .text(", and vkd3d-proton gave ")
            .number(image.size())
            .text(" of type ")
            .number(image.memory_type())
            .write();
        vulkan_.table().destroy_image(made);
        return nullptr;
    }
    const int fd = image.take_fd();
    const VkImportMemoryFdInfoKHR imported = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr,
                                              VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT, fd};
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!vulkan_.table().bind_dedicated(made, &imported, image.size(), image.memory_type(), memory)) {
        refuse(memory == VK_NULL_HANDLE ? "the memory's fd was not imported" : "the imported memory was not bound");
        /* An import that failed never took the fd. */
        if (memory == VK_NULL_HANDLE) close(fd);
        vulkan_.table().free_memory(memory);
        vulkan_.table().destroy_image(made);
        return nullptr;
    }
    return wrap(made, memory, drm_format_mod_invalid, desc);
}

/* The same description the compositor made the image from, so both devices
 * read one layout from the memory; dedicated, as the compositor's is. */
ID3D11Texture2D *ExportableImages::adopt(int fd, const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("ExportableImages::adopt");
    const ExportableImageInfo described(desc.Width, desc.Height, desc.MipLevels, desc.ArraySize,
                                        desc.SampleDesc.Count, static_cast<uint32_t>(desc.Format), desc.BindFlags);
    return adopt_laid_out(fd, described, drm_format_mod_linear, desc);
}

ID3D11Texture2D *ExportableImages::adopt(int fd, const D3D11_TEXTURE2D_DESC1 &desc, uint64_t modifier,
                                         uint64_t offset, uint64_t pitch)
{
    AV_SCOPE("ExportableImages::adopt");
    const ExportableImageInfo described(desc.Width, desc.Height, desc.MipLevels, desc.ArraySize,
                                        desc.SampleDesc.Count, static_cast<uint32_t>(desc.Format), desc.BindFlags,
                                        modifier, offset, pitch);
    return adopt_laid_out(fd, described, modifier, desc);
}

ID3D11Texture2D *ExportableImages::adopt_laid_out(int fd, const ExportableImageInfo &described, uint64_t modifier,
                                                  const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("ExportableImages::adopt_laid_out");
    if (fd < 0) return nullptr;
    VkImage made = VK_NULL_HANDLE;
    if (!described.supported() || !vulkan_.connected() || !vulkan_.table().create_image(described.info(), made)) {
        close(fd);
        return nullptr;
    }
    collect();
    const VkMemoryRequirements    needs = vulkan_.table().requirements(made);
    const uint32_t                type = vulkan_.table().memory_type(needs.memoryTypeBits);
    const VkImportMemoryFdInfoKHR imported = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr,
                                              VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd};
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (type == VK_MAX_MEMORY_TYPES || !vulkan_.table().bind_dedicated(made, &imported, needs.size, type, memory)) {
        lines_.name("an image cannot be imported:", "the dma-buf was not imported");
        /* Memory that was allocated took the fd; otherwise it is still ours. */
        if (memory == VK_NULL_HANDLE) close(fd);
        vulkan_.table().free_memory(memory);
        vulkan_.table().destroy_image(made);
        return nullptr;
    }
    return wrap(made, memory, modifier, desc);
}

/* Rows, over memory whose bytes are kept as they are: an OpenCL that names no
 * image over a buffer opens the texture as its bytes
 * (OpenClBackend::image_over). A GPU that cannot draw into rows of the format
 * leaves the texture to DXVK, with memory nothing else opens: its pictures
 * then cross as pixels. */
ID3D11Texture2D *ExportableImages::make_rows(const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("ExportableImages::make_rows");
    const ExportableImageInfo described(desc.Width, desc.Height, desc.MipLevels, desc.ArraySize,
                                        desc.SampleDesc.Count, static_cast<uint32_t>(desc.Format), desc.BindFlags);
    if (!described.supported() || !vulkan_.connected() || !vulkan_.table().supports_exportable(described.info())) return nullptr;
    collect();
    VkImage made = VK_NULL_HANDLE;
    if (!vulkan_.table().create_image(described.info(), made)) return nullptr;
    const uint32_t type = vulkan_.table().raw_memory_type(vulkan_.table().requirements(made).memoryTypeBits);
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!bind_exported(made, type, memory)) return nullptr;
    return wrap(made, memory, drm_format_mod_linear, desc);
}

/* Laid out by a tiling the driver picks among the GPU's own, over memory of
 * the GPU's choosing: what Direct3D draws and samples at full speed. On
 * Intel's Xe graphics a full-screen draw into rows takes three and a half
 * times as long as into the same image tiled, and one from rows six times
 * (tools/host/shared_image_bench.c): DWM composing a captured monitor into
 * shared rows and from them every frame was what dropped its frames. A GPU
 * with no tiling it exports in one plane gets rows. */
ID3D11Texture2D *ExportableImages::make_tiled(const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("ExportableImages::make_tiled");
    const ExportableImageInfo rows(desc.Width, desc.Height, desc.MipLevels, desc.ArraySize, desc.SampleDesc.Count,
                                   static_cast<uint32_t>(desc.Format), desc.BindFlags);
    if (!rows.supported() || !vulkan_.connected()) return nullptr;
    uint64_t       tilings[most_tilings] = {};
    const uint32_t count = vulkan_.table().tiled_modifiers(rows.info(), tilings, most_tilings);
    if (count == 0u) return make_rows(desc);
    const ExportableImageInfo described(desc.Width, desc.Height, desc.MipLevels, desc.ArraySize,
                                        desc.SampleDesc.Count, static_cast<uint32_t>(desc.Format), desc.BindFlags,
                                        tilings, count);
    collect();
    VkImage made = VK_NULL_HANDLE;
    if (!vulkan_.table().create_image(described.info(), made)) return make_rows(desc);
    const uint32_t type = vulkan_.table().memory_type(vulkan_.table().requirements(made).memoryTypeBits);
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (!bind_exported(made, type, memory)) return make_rows(desc);
    const uint64_t modifier = vulkan_.table().modifier_of(made);
    if (modifier == drm_format_mod_invalid) {
        vulkan_.table().free_memory(memory);
        vulkan_.table().destroy_image(made);
        return make_rows(desc);
    }
    return wrap(made, memory, modifier, desc);
}

/* Memory of `type` for `image` alone, exportable as a dma-buf; on failure
 * the image is destroyed with whatever was allocated. */
bool ExportableImages::bind_exported(VkImage image, uint32_t type, VkDeviceMemory &memory)
{
    AV_SCOPE("ExportableImages::bind_exported");
    memory = VK_NULL_HANDLE;
    const VkExportMemoryAllocateInfo exported = {VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, nullptr,
                                                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    if (type != VK_MAX_MEMORY_TYPES &&
        vulkan_.table().bind_dedicated(image, &exported, vulkan_.table().requirements(image).size, type, memory))
        return true;
    vulkan_.table().free_memory(memory);
    vulkan_.table().destroy_image(image);
    memory = VK_NULL_HANDLE;
    return false;
}

bool ExportableImages::row_pitch(ID3D11Texture2D *texture, uint32_t &pitch)
{
    AV_SCOPE("ExportableImages::row_pitch");
    pitch = 0u;
    const ExportableImage *const made = image_of(texture);
    if (made == nullptr || !made->rows()) return false;
    pitch = static_cast<uint32_t>(vulkan_.table().row_layout(made->image()).rowPitch);
    return pitch != 0u;
}

bool ExportableImages::carry(ID3D11Texture2D *texture, int &fd, uint64_t &modifier, uint64_t &offset, uint64_t &pitch)
{
    AV_SCOPE("ExportableImages::carry");
    fd = -1;
    const ExportableImage *const image = image_of(texture);
    if (image == nullptr || image->modifier() == drm_format_mod_invalid ||
        !vulkan_.table().memory_fd(image->memory(), VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd))
        return false;
    const VkSubresourceLayout laid = vulkan_.table().row_layout(image->image());
    modifier = image->modifier();
    offset = laid.offset;
    pitch = laid.rowPitch;
    return true;
}

bool ExportableImages::export_memory(ID3D11Texture2D *texture, int &fd)
{
    AV_SCOPE("ExportableImages::export_memory");
    fd = -1;
    const ExportableImage *const image = image_of(texture);
    return image != nullptr && image->rows() &&
           vulkan_.table().memory_fd(image->memory(), VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd);
}

/* The image is laid out by a modifier (ExportableImageInfo), so its one
 * plane's layout is the driver's to report and the importer's to follow,
 * under that modifier. */
bool ExportableImages::export_image(ID3D11Texture2D *texture, AvExportedImage &image)
{
    AV_SCOPE("ExportableImages::export_image");
    image = AvExportedImage{-1, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    const ExportableImage *const made = image_of(texture);
    if (made == nullptr || made->modifier() == drm_format_mod_invalid) return false;
    D3D11_TEXTURE2D_DESC described = {};
    texture->GetDesc(&described);
    const uint32_t fourcc = DxgiFormat(static_cast<uint32_t>(described.Format)).drm_fourcc();
    int            fd = -1;
    if (fourcc == 0u || !vulkan_.table().memory_fd(made->memory(), VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd))
        return false;
    const VkSubresourceLayout laid = vulkan_.table().row_layout(made->image());
    image.fd = fd;
    image.width = described.Width;
    image.height = described.Height;
    image.stride = static_cast<uint32_t>(laid.rowPitch);
    image.offset = static_cast<uint32_t>(laid.offset);
    image.fourcc = fourcc;
    image.modifier = made->modifier();
    return true;
}

void ExportableImages::collect()
{
    AV_SCOPE("ExportableImages::collect");
    if (__atomic_exchange_n(&retired_, 0u, __ATOMIC_ACQ_REL) == 0u) return;
    vulkan_.idle();
    images_.destroy_if([this](const ExportableImage &image) {
        if (!image.retired()) return false;
        const_cast<ExportableImage &>(image).free(vulkan_);
        return true;
    });
}

void ExportableImages::release()
{
    AV_SCOPE("ExportableImages::release");
    if (images_.live() == 0u) return;
    vulkan_.idle();
    images_.destroy_if([this](const ExportableImage &image) {
        const_cast<ExportableImage &>(image).free(vulkan_);
        return true;
    });
    __atomic_store_n(&retired_, 0u, __ATOMIC_RELEASE);
}

void ExportableImages::retired()
{
    AV_SCOPE("ExportableImages::retired");
    __atomic_fetch_add(&retired_, 1u, __ATOMIC_ACQ_REL);
}

void ExportableImages::report(StateWriter &writer) const
{
    AV_SCOPE("ExportableImages::report");
    uint32_t waiting = 0u;
    images_.for_each([&waiting](const ExportableImage &image) {
        if (image.retired()) waiting++;
    });
    writer.open("exportable_images", 0u);
    writer.number("live", images_.live());
    writer.number("retired", waiting);
    writer.number("blocks", heap_.live());
    writer.close();
}

ExportableImage *ExportableImages::image_of(ID3D11Texture2D *texture)
{
    AV_SCOPE("ExportableImages::image_of");
    if (texture == nullptr) return nullptr;
    IUnknown *held = nullptr;
    UINT      size = sizeof held;
    if (FAILED(texture->GetPrivateData(image_data, &size, &held)) || held == nullptr) return nullptr;
    /* The reference the read took is the image's own count, given straight back. */
    held->Release();
    /* The pool knows its images by their own addresses, which are the
     * hold's: it is their only base. */
    ExportableImage *const image = images_.find(static_cast<void *>(held));
    return image != nullptr && !image->retired() ? image : nullptr;
}

ID3D11Texture2D *ExportableImages::wrap(VkImage image, VkDeviceMemory memory, uint64_t modifier,
                                        const D3D11_TEXTURE2D_DESC1 &desc)
{
    AV_SCOPE("ExportableImages::wrap");
    ExportableImage *made = nullptr;
    if (images_.create(made, *this, image, memory, modifier) != Status::ok) {
        lines_.write("an exportable image cannot be recorded: out of memory");
        vulkan_.table().free_memory(memory);
        vulkan_.table().destroy_image(image);
        return nullptr;
    }
    ID3D11Texture2D *texture = nullptr;
    if (!vulkan_.wrap(desc, image, texture) ||
        FAILED(texture->SetPrivateDataInterface(image_data,
                                                reinterpret_cast<IUnknown *>(static_cast<TextureHold *>(made))))) {
        lines_.write("DXVK made no texture over an image of its own device");
        if (texture != nullptr) texture->Release();
        /* Collected with the rest when the texture held it; freed here when not. */
        if (!made->retired()) {
            made->free(vulkan_);
            static_cast<void>(images_.destroy(made));
        }
        return nullptr;
    }
    return texture;
}

}  // namespace av
