// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d9.toml --out ...

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace av::d3d9proto {

struct Rect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};
static_assert(sizeof(Rect) == 16, "Rect crosses as 16 bytes");
static_assert(__is_trivially_copyable(Rect), "Rect is plain data");

struct Box {
    uint32_t left;
    uint32_t top;
    uint32_t front;
    uint32_t right;
    uint32_t bottom;
    uint32_t back;
};
static_assert(sizeof(Box) == 24, "Box crosses as 24 bytes");
static_assert(__is_trivially_copyable(Box), "Box is plain data");

struct Viewport {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    float min_z;
    float max_z;
};
static_assert(sizeof(Viewport) == 24, "Viewport crosses as 24 bytes");
static_assert(__is_trivially_copyable(Viewport), "Viewport is plain data");

struct VertexElement {
    uint32_t stream;
    uint32_t offset;
    uint32_t type;
    uint32_t method;
    uint32_t usage;
    uint32_t usage_index;
};
static_assert(sizeof(VertexElement) == 24, "VertexElement crosses as 24 bytes");
static_assert(__is_trivially_copyable(VertexElement), "VertexElement is plain data");

struct Material {
    float diffuse[4];
    float ambient[4];
    float specular[4];
    float emissive[4];
    float power;
};
static_assert(sizeof(Material) == 68, "Material crosses as 68 bytes");
static_assert(__is_trivially_copyable(Material), "Material is plain data");

struct Light {
    uint32_t type;
    float diffuse[4];
    float specular[4];
    float ambient[4];
    float position[3];
    float direction[3];
    float range;
    float falloff;
    float attenuation[3];
    float theta;
    float phi;
};
static_assert(sizeof(Light) == 104, "Light crosses as 104 bytes");
static_assert(__is_trivially_copyable(Light), "Light is plain data");

struct FormatCaps9 {
    uint32_t support;
    uint32_t multisample;
    uint32_t quality_levels[16];
};
static_assert(sizeof(FormatCaps9) == 72, "FormatCaps9 crosses as 72 bytes");
static_assert(__is_trivially_copyable(FormatCaps9), "FormatCaps9 is plain data");

}  // namespace av::d3d9proto
