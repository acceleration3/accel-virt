#ifndef AV_UM_D3D12_STATE_OBJECT_WIRE_HPP
#define AV_UM_D3D12_STATE_OBJECT_WIRE_HPP

#include "core/support/growable_array.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/d3d12_ddi.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::d3d12 {

class D3d12StateObjectWire {
public:
    static constexpr uint32_t most_subobjects = 1024u;
    static constexpr uint32_t most_exports = 16384u;
    static constexpr uint32_t most_associations = 16384u;
    static constexpr uint32_t no_name = 0xFFFFFFFFu;

    explicit D3d12StateObjectWire(BlockSource &memory);
    D3d12StateObjectWire(const D3d12StateObjectWire &) = delete;
    D3d12StateObjectWire &operator=(const D3d12StateObjectWire &) = delete;

    [[nodiscard]] bool read(const D3D12DDI_STATE_SUBOBJECT_0054 *subobjects, UINT count);

    uint32_t                                subobject_count() const;
    const d3d12proto::StateSubobject       *subobjects() const;
    uint32_t                                export_count() const;
    const d3d12proto::StateExport          *exports() const;
    uint32_t                                association_count() const;
    const d3d12proto::StateAssociation     *associations() const;
    const uint8_t                          *names() const;
    uint64_t                                name_bytes() const;
    const uint8_t                          *libraries() const;
    uint64_t                                library_bytes() const;

private:
    [[nodiscard]] bool     add_name(LPCWSTR name, uint32_t &offset);
    [[nodiscard]] bool     add_exports(const D3D12DDI_EXPORT_DESC_0054 *exports, UINT count, uint32_t &first);
    [[nodiscard]] bool     add_library(const D3D12DDI_DXIL_LIBRARY_DESC_0054 &library, d3d12proto::StateSubobject &out);
    [[nodiscard]] bool     add_hit_group(const D3D12DDI_HIT_GROUP_DESC_0054 &group, d3d12proto::StateSubobject &out);
    [[nodiscard]] bool     add_associations(const D3D12DDI_FUNCTION_SUMMARY_0054 &summary,
                                            const D3D12DDI_STATE_SUBOBJECT_0054 *subobjects, UINT count);
    [[nodiscard]] bool     convert(const D3D12DDI_STATE_SUBOBJECT_0054 &from, d3d12proto::StateSubobject &out);
    static uint32_t        object_id(const void *block);
    uint32_t               index_of(const D3D12DDI_STATE_SUBOBJECT_0054 *wanted,
                                    const D3D12DDI_STATE_SUBOBJECT_0054 *subobjects, UINT count) const;

    GrowableArray<d3d12proto::StateSubobject>   subobjects_;
    GrowableArray<d3d12proto::StateExport>      exports_;
    GrowableArray<d3d12proto::StateAssociation> associations_;
    GrowableArray<uint16_t>                     names_;
    GrowableArray<uint8_t>                      libraries_;
    GrowableArray<uint32_t>                     wire_of_;
};

}

#endif
