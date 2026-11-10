#ifndef AV_D3D12_STATE_OBJECT_BUILDER_HPP
#define AV_D3D12_STATE_OBJECT_BUILDER_HPP

#include "core/support/growable_array.hpp"
#include "executor/shim/d3d12_raytracing_types.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

namespace av {

class BlockSource;
class D3dObjectTable;

class D3d12StateObjectBuilder {
public:
    static constexpr uint32_t most_subobjects = 1024u;
    static constexpr uint32_t most_exports = 16384u;
    static constexpr uint32_t most_associations = 16384u;
    static constexpr uint64_t most_name_bytes = uint64_t{1} << 24u;
    static constexpr uint64_t most_library_bytes = uint64_t{1} << 30u;
    static constexpr uint32_t no_name = 0xFFFFFFFFu;

    struct Wire {
        uint32_t                          type;
        uint32_t                          subobject_count;
        const d3d12proto::StateSubobject *subobjects;
        uint32_t                          export_count;
        const d3d12proto::StateExport    *exports;
        uint32_t                          association_count;
        const d3d12proto::StateAssociation *associations;
        const uint8_t                    *names;
        uint64_t                          names_size;
        const uint8_t                    *libraries;
        uint64_t                          libraries_size;
    };

    D3d12StateObjectBuilder(BlockSource &blocks, const D3dObjectTable &objects);
    D3d12StateObjectBuilder(const D3d12StateObjectBuilder &) = delete;
    D3d12StateObjectBuilder &operator=(const D3d12StateObjectBuilder &) = delete;

    [[nodiscard]] bool read(const Wire &wire);

    const d3d12new::StateObjectDesc &desc() const;

private:
    union Slot {
        d3d12new::StateObjectConfig     config;
        d3d12new::RootSignatureSubobject root;
        d3d12new::NodeMaskSubobject     node;
        d3d12new::LibraryDesc           library;
        d3d12new::CollectionDesc        collection;
        d3d12new::ShaderConfigDesc      shader;
        d3d12new::PipelineConfigDesc    pipeline;
        d3d12new::PipelineConfig1Desc   pipeline1;
        d3d12new::HitGroupDesc          hit;
        d3d12new::AssociationDesc       association;
    };

    [[nodiscard]] bool        copy_names(const Wire &wire);
    const char16_t           *name_at(uint32_t offset) const;
    [[nodiscard]] bool        make_exports(const Wire &wire);
    [[nodiscard]] bool        reserve_wrapping(const Wire &wire);
    [[nodiscard]] bool        library_code(const Wire &wire, const d3d12proto::StateSubobject &from,
                                           d3d12new::ShaderCode &code);
    [[nodiscard]] bool        export_range(const Wire &wire, const d3d12proto::StateSubobject &from, uint32_t &count,
                                           const d3d12new::ExportDesc *&first) const;
    [[nodiscard]] bool        make_subobject(const Wire &wire, uint32_t index);
    [[nodiscard]] bool        make_associations(const Wire &wire);

    const D3dObjectTable                 &objects_;
    GrowableArray<char16_t>               names_;
    GrowableArray<d3d12new::ExportDesc>   exports_;
    GrowableArray<uint8_t>                wrapped_;
    uint64_t                              wrapped_used_;
    GrowableArray<Slot>                   slots_;
    GrowableArray<d3d12new::StateSubobject> subobjects_;
    GrowableArray<const char16_t *>       association_names_;
    d3d12new::StateObjectDesc             desc_;
};

}

#endif
