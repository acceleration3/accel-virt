/* One shared Direct3D 11 resource carried across its device's loss: what it
 * was, read off it while the lost device's objects still stand, and made
 * again on the device made in its place.
 *
 * The guest's allocation outlives a host device: on Windows a resource shared
 * before a TDR still opens after it, and DWM, making its device again, opens
 * every client's surface again - a refusal it treats as its new device
 * failing, which it makes again, and again, until it is restarted. So a
 * shared resource is made again under the same share: over the same memory
 * where it was made over memory other APIs open (ExportableImages), its
 * pixels kept, and a new one of its description otherwise. */
#ifndef AV_CARRIED_SHARE_HPP
#define AV_CARRIED_SHARE_HPP

#include "executor/owned_fd.hpp"

#include <stdint.h>

struct ID3D11Device5;
struct IUnknown;

namespace av {

class ExportableImages;

class CarriedShare {
public:
    CarriedShare(uint32_t domain, uint64_t id);
    ~CarriedShare();
    CarriedShare(const CarriedShare &) = delete;
    CarriedShare &operator=(const CarriedShare &) = delete;

    /* What `resource` is: its description, and a dma-buf of its memory where
     * `images` made it. False for a resource of no kind Direct3D 11 shares. */
    [[nodiscard]] bool record(IUnknown *resource, ExportableImages &images);
    /* The resource made again on `device`, marked as the guest's surface,
     * with a reference the caller owns; null when the device makes none. */
    IUnknown          *remake(ID3D11Device5 *device, ExportableImages &images);
    uint32_t           domain() const;
    uint64_t           id() const;

    bool invariants_hold() const;

private:
    enum class Dimension : uint32_t { none, buffer, texture1d, texture2d, texture3d };

    IUnknown *remake_texture2d(ID3D11Device5 *device, ExportableImages &images);

    uint64_t  id_;
    uint32_t  domain_;
    Dimension dimension_;
    /* The description's fields as Direct3D 11 names them: a buffer's byte
     * width and structure stride, a texture's extent, levels and layers. */
    uint32_t  width_;
    uint32_t  height_;
    uint32_t  depth_or_array_;
    uint32_t  mips_;
    uint32_t  format_;
    uint32_t  samples_;
    uint32_t  quality_;
    uint32_t  bind_;
    uint32_t  misc_;
    uint32_t  stride_;
    /* The memory it was made over, where ExportableImages made it; nothing
     * for none. Taken by a remake over it. */
    OwnedFd   fd_;
    /* How that memory is laid out: the modifier, and its one plane's start
     * and pitch. */
    uint64_t  modifier_;
    uint64_t  offset_;
    uint64_t  pitch_;
};

}  // namespace av

#endif  // AV_CARRIED_SHARE_HPP
