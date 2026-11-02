#ifndef AV_EXPORTABLE_IMAGES_HPP
#define AV_EXPORTABLE_IMAGES_HPP

#include "core/support/object_pool.hpp"
#include "device/abi/av_executor.h"
#include "executor/shim/exportable_image.hpp"
#include "executor/shim/system_memory_twin.hpp"
#include "executor/shim/host_heap.hpp"

#include <stdint.h>
#include <vulkan/vulkan.h>

struct ID3D11DeviceContext4;
struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC1;

namespace av {

class DriverLog;
class OwnedFd;
class DxvkVulkanDevice;
class ExportableImageInfo;
class StateWriter;
class Vkd3dImageExport;

class ExportableImages {
public:
    ExportableImages(DxvkVulkanDevice &vulkan, DriverLog &lines);
    ~ExportableImages();
    ExportableImages(const ExportableImages &) = delete;
    ExportableImages &operator=(const ExportableImages &) = delete;

    ID3D11Texture2D *import(Vkd3dImageExport &image, const D3D11_TEXTURE2D_DESC1 &desc);
    ID3D11Texture2D *make_rows(const D3D11_TEXTURE2D_DESC1 &desc);
    ID3D11Texture2D *make_tiled(const D3D11_TEXTURE2D_DESC1 &desc);
    ID3D11Texture2D *adopt(OwnedFd &descriptor, const D3D11_TEXTURE2D_DESC1 &desc);
    ID3D11Texture2D *adopt(OwnedFd &descriptor, const D3D11_TEXTURE2D_DESC1 &desc, uint64_t modifier,
                           uint64_t offset, uint64_t pitch);
    [[nodiscard]] bool carry(ID3D11Texture2D *texture, OwnedFd &descriptor, uint64_t &modifier, uint64_t &offset,
                             uint64_t &pitch);
    [[nodiscard]] bool export_memory(ID3D11Texture2D *texture, OwnedFd &descriptor);
    [[nodiscard]] bool row_pitch(ID3D11Texture2D *texture, uint32_t &pitch);
    [[nodiscard]] bool export_image(ID3D11Texture2D *texture, AvExportedImage &image);
    [[nodiscard]] bool export_twin(ID3D11Texture2D *texture, AvExportedImage &image);
    [[nodiscard]] bool refresh_twin(ID3D11DeviceContext4 *context, ID3D11Texture2D *texture, uint64_t writes);
    void             collect();
    void             release();
    void             retired();
    void             report(StateWriter &writer) const;

private:
    ExportableImage *image_of(ID3D11Texture2D *texture);
    SystemMemoryTwin *twin_of(ID3D11Texture2D *texture);
    SystemMemoryTwin *make_twin(ID3D11Texture2D *texture);
    ExportableImage *record();
    void             discard(ExportableImage &made);
    ID3D11Texture2D *adopt_laid_out(OwnedFd &descriptor, const ExportableImageInfo &described, uint64_t modifier,
                                    const D3D11_TEXTURE2D_DESC1 &desc);
    ID3D11Texture2D *wrap(ExportableImage &made, uint64_t modifier, const D3D11_TEXTURE2D_DESC1 &desc);

    DxvkVulkanDevice           &vulkan_;
    HostHeap                    heap_;
    ObjectPool<ExportableImage> images_;
    ObjectPool<SystemMemoryTwin> twins_;
    bool                        twin_refused_;
    uint32_t                    retired_;
    DriverLog                  &lines_;
};

}

#endif
