#ifndef AV_D3D12_RAYTRACING_TYPES_HPP
#define AV_D3D12_RAYTRACING_TYPES_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::d3d12new {

struct ShaderCode {
    const void *code;
    size_t      bytes;
};

struct StateSubobject {
    int         type;
    const void *desc;
};

struct StateObjectDesc {
    int                   type;
    uint32_t              count;
    const StateSubobject *subobjects;
};

struct StateObjectConfig {
    uint32_t flags;
};

struct RootSignatureSubobject {
    void *signature;
};

struct NodeMaskSubobject {
    uint32_t mask;
};

struct ExportDesc {
    const char16_t *name;
    const char16_t *rename;
    uint32_t        flags;
};

struct LibraryDesc {
    ShaderCode        code;
    uint32_t          export_count;
    const ExportDesc *exports;
};

struct CollectionDesc {
    void             *collection;
    uint32_t          export_count;
    const ExportDesc *exports;
};

struct AssociationDesc {
    const StateSubobject  *subobject;
    uint32_t               export_count;
    const char16_t *const *exports;
};

struct ShaderConfigDesc {
    uint32_t payload_bytes;
    uint32_t attribute_bytes;
};

struct PipelineConfigDesc {
    uint32_t max_depth;
};

struct PipelineConfig1Desc {
    uint32_t max_depth;
    uint32_t flags;
};

struct HitGroupDesc {
    const char16_t *export_name;
    int             type;
    const char16_t *any_hit;
    const char16_t *closest_hit;
    const char16_t *intersection;
};

struct GpuAndStride {
    uint64_t start;
    uint64_t stride;
};

struct GpuRange {
    uint64_t start;
    uint64_t size;
};

struct GpuRangeAndStride {
    uint64_t start;
    uint64_t size;
    uint64_t stride;
};

struct TrianglesDesc {
    uint64_t     transform;
    uint32_t     index_format;
    uint32_t     vertex_format;
    uint32_t     index_count;
    uint32_t     vertex_count;
    uint64_t     index_buffer;
    GpuAndStride vertices;
};

struct AabbsDesc {
    uint64_t     count;
    GpuAndStride boxes;
};

struct GeometryDesc {
    int      type;
    uint32_t flags;
    union {
        TrianglesDesc triangles;
        AabbsDesc     aabbs;
    };
};

struct BuildInputs {
    int      type;
    uint32_t flags;
    uint32_t count;
    int      layout;
    union {
        uint64_t            instances;
        const GeometryDesc *geometries;
    };
};

struct BuildDesc {
    uint64_t    destination;
    BuildInputs inputs;
    uint64_t    source;
    uint64_t    scratch;
};

struct PrebuildInfo {
    uint64_t result_bytes;
    uint64_t scratch_bytes;
    uint64_t update_scratch_bytes;
};

struct PostbuildDesc {
    uint64_t destination;
    int      type;
};

struct DispatchRaysDesc {
    GpuRange          raygen;
    GpuRangeAndStride miss;
    GpuRangeAndStride hit;
    GpuRangeAndStride callable;
    uint32_t          width;
    uint32_t          height;
    uint32_t          depth;
};

}

#endif
