#ifndef AV_D3D9_SLICES_HPP
#define AV_D3D9_SLICES_HPP

#include "executor/d3d/slice_bindings.hpp"
#include "executor/shim/d3d9_slice_device.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DResource9;

namespace av {

class D3d9BatchProgress;
class D3d9HostDevice;
class DriverLog;
class ImportTable;
class SliceStorages;
class SourceMemory;

class D3d9Slices {
public:
    D3d9Slices(SliceStorages &storages, D3d9BatchProgress &progress, DriverLog &lines);
    D3d9Slices(const D3d9Slices &) = delete;
    D3d9Slices &operator=(const D3d9Slices &) = delete;

    void initialize(D3d9HostDevice *host, ImportTable *imports);
    void shut_down();

    bool               supported() const;
    bool               active() const;
    [[nodiscard]] bool bind(IDirect3DDevice9 *device, uint32_t resource, IDirect3DResource9 *buffer, uint64_t memory,
                            uint64_t offset, uint64_t bytes);
    [[nodiscard]] bool detach(uint32_t resource, IDirect3DResource9 *buffer);
    void               discard(IDirect3DResource9 *buffer);
    void               forget(uint32_t resource);
    void               forget_device(IDirect3DDevice9 *device);
    void               reclaim();

    bool invariants_hold() const;

private:
    bool                 fits(IDirect3DResource9 *buffer, uint64_t bytes) const;
    [[nodiscard]] bool   leave(IDirect3DResource9 *buffer, const SliceBindings::Binding &was);
    [[nodiscard]] bool   lock_fresh(IDirect3DResource9 *buffer, void *&mapped, uint32_t &size) const;
    void                 unlock(IDirect3DResource9 *buffer) const;
    void                 settle();

    SliceStorages     &storages_;
    D3d9BatchProgress &progress_;
    DriverLog         &lines_;
    D3d9HostDevice    *host_;
    ImportTable       *imports_;
    D3d9SliceDevice    lane_;
    bool               reported_;
};

}

#endif
