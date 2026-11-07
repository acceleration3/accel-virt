#ifndef AV_D3D9_LANE_HPP
#define AV_D3D9_LANE_HPP

#include "executor/slice_progress.hpp"
#include "executor/shim/d3d9_interop.hpp"
#include "executor/shim/slice_importer.hpp"
#include "executor/shim/vulkan_device_table.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DResource9;

namespace av {

class D3d9Lanes;

class D3d9Lane final : public SliceImporter, public SliceProgress {
public:
    D3d9Lane(IDirect3DDevice9 *device, const D3d9Lanes &lanes);
    ~D3d9Lane();
    D3d9Lane(const D3d9Lane &) = delete;
    D3d9Lane &operator=(const D3d9Lane &) = delete;

    [[nodiscard]] bool connect();
    bool               connected() const;
    bool               is(const IDirect3DDevice9 *device) const;
    bool               is(const SliceImporter *importer) const;
    void               allow();
    bool               allowed() const;
    void               touch();
    bool               touched() const;
    [[nodiscard]] bool mark(uint64_t value, int wake);
    void               completed(uint64_t value);
    bool               polled() const;
    void               idle() const;
    void               release();

    [[nodiscard]] bool import_to_draw(OwnedFd &descriptor, uint64_t bytes, VulkanBuffer &buffer) override;
    [[nodiscard]] bool bindable(IDirect3DResource9 *buffer) const;
    [[nodiscard]] bool bind_storage(IDirect3DResource9 *buffer, const VulkanBuffer &storage, uint64_t offset,
                                    uint64_t bytes, void *host) const;
    [[nodiscard]] bool unbind_storage(IDirect3DResource9 *buffer) const;

    uint64_t next() const override;
    uint64_t reached() const override;

    bool invariants_hold() const;

private:
    friend struct D3d9LaneProbe;

    IDirect3DDevice9 *device_;
    const D3d9Lanes  &lanes_;
    D3d9Interop       interop_;
    VulkanDeviceTable table_;
    uint64_t          issued_;
    uint64_t          done_;
    int               wake_;
    bool              tried_;
    bool              allowed_;
    bool              touched_;
};

}

#endif
