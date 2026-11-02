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

    [[nodiscard]] bool record(IUnknown *resource, ExportableImages &images);
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
    OwnedFd   fd_;
    uint64_t  modifier_;
    uint64_t  offset_;
    uint64_t  pitch_;
};

}

#endif
