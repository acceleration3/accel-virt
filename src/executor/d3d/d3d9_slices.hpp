#ifndef AV_D3D9_SLICES_HPP
#define AV_D3D9_SLICES_HPP

#include "executor/d3d/d3d9_copy_bindings.hpp"
#include "executor/d3d/slice_bindings.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DResource9;

namespace av {

class BlockSource;
class D3d9HostDevice;
class D3d9Lane;
class D3d9Lanes;
class DriverLog;
class ImportTable;
class SliceStorages;
class SourceMemory;

class D3d9Slices {
public:
    D3d9Slices(BlockSource &blocks, SliceStorages &storages, D3d9Lanes &lanes, DriverLog &lines);
    D3d9Slices(const D3d9Slices &) = delete;
    D3d9Slices &operator=(const D3d9Slices &) = delete;

    void initialize(D3d9HostDevice *host, ImportTable *imports);
    void shut_down();

    bool               supported() const;
    [[nodiscard]] bool probe(IDirect3DDevice9 *device);
    [[nodiscard]] bool bind(IDirect3DDevice9 *device, uint32_t resource, IDirect3DResource9 *buffer, uint64_t memory,
                            uint64_t offset, uint64_t bytes);
    [[nodiscard]] bool detach(uint32_t resource, IDirect3DResource9 *buffer);
    void               copy_in_use(IDirect3DDevice9 *device);
    [[nodiscard]] bool stream_bound(IDirect3DDevice9 *device, uint32_t stream, uint32_t resource);
    [[nodiscard]] bool indices_bound(IDirect3DDevice9 *device, uint32_t resource);
    void               discard(const SliceBindings::Binding &bound, IDirect3DResource9 *buffer);
    void               forget(uint32_t resource);
    void               forget_device(IDirect3DDevice9 *device);
    void               reclaim();

    bool invariants_hold() const;

private:
    static constexpr uint64_t offset_alignment = 4u;

    [[nodiscard]] bool bound_to_storage(D3d9Lane &lane, IDirect3DResource9 *buffer, uint32_t resource,
                                        SourceMemory &store, uint8_t *slice, uint64_t offset, uint64_t bytes);
    [[nodiscard]] bool copy_at_draw(IDirect3DDevice9 *device, uint32_t resource, IDirect3DResource9 *buffer,
                                    SourceMemory &store, uint8_t *slice, uint64_t bytes);
    void               copy_one(uint32_t resource, IDirect3DResource9 *buffer);
    bool               fits(IDirect3DResource9 *buffer, uint64_t bytes) const;
    [[nodiscard]] bool leave(IDirect3DResource9 *buffer, const SliceBindings::Binding &was);
    [[nodiscard]] bool lock_for(IDirect3DResource9 *buffer, uint32_t flags, void *&mapped, uint32_t &size) const;
    void               unlock(IDirect3DResource9 *buffer) const;

    SliceStorages                 &storages_;
    D3d9Lanes                     &lanes_;
    DriverLog                     &lines_;
    D3d9HostDevice                *host_;
    ImportTable                   *imports_;
    D3d9CopyBindings               copies_;
    bool                           reported_;
};

}

#endif
