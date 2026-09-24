/* Descriptors written from the wire's view descriptions.
 *
 * The guest's driver describes a view as its dimension and that dimension's
 * union members in declaration order (d3d12.toml, ViewDesc); this turns one
 * into the Direct3D 12 description of its kind, with the resource the guest's
 * id names, and writes it at a descriptor handle the session resolved. A
 * dimension the kind has not got, a count past the members it has, or a
 * resource id naming nothing is refused - nothing is written then. */
#ifndef AV_D3D12_VIEW_WRITER_HPP
#define AV_D3D12_VIEW_WRITER_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stddef.h>
#include <stdint.h>

struct ID3D12Device;
struct ID3D12Resource;

namespace av {

class D3d12AddressMap;
class D3dObjectTable;

class D3d12ViewWriter {
public:
    D3d12ViewWriter(const D3dObjectTable &objects, const D3d12AddressMap &addresses);

    /* The device views are written on; null refuses everything. */
    void bind(ID3D12Device *device);

    /* has_desc false is the null description (the resource's own format and
     * whole extent); `at` is a CPU descriptor handle of the right heap type. */
    [[nodiscard]] bool shader_resource(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const;
    [[nodiscard]] bool unordered_access(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const;
    [[nodiscard]] bool render_target(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const;
    [[nodiscard]] bool depth_stencil(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const;
    /* A constant buffer view of `size` bytes at a guest address. */
    [[nodiscard]] bool constant_buffer(size_t at, uint64_t address, uint32_t size) const;
    [[nodiscard]] bool sampler(size_t at, const d3d12proto::SamplerDesc &desc) const;

private:
    ID3D12Resource *resource(uint32_t id) const;
    /* `count` of the view's args, then (when `lod`) its min_lod, into the
     * union at `members`; false for a count past the args the wire has. */
    bool            members(void *members, uint32_t count, bool lod, const d3d12proto::ViewDesc &desc) const;

    const D3dObjectTable  &objects_;
    const D3d12AddressMap &addresses_;
    ID3D12Device          *device_;
};

}  // namespace av

#endif  // AV_D3D12_VIEW_WRITER_HPP
