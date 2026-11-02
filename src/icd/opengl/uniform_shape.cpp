#include "icd/opengl/uniform_shape.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

namespace {
struct TypeValues {
    uint32_t type_;
    uint32_t values_;
};

constexpr TypeValues shaped_types[] = {
    {0x1404u, 1u},  {0x1405u, 1u},  {0x1406u, 1u},  {0x140Au, 1u},
    {0x8B50u, 2u},  {0x8B51u, 3u},  {0x8B52u, 4u},
    {0x8B53u, 2u},  {0x8B54u, 3u},  {0x8B55u, 4u},
    {0x8B56u, 1u},  {0x8B57u, 2u},  {0x8B58u, 3u},  {0x8B59u, 4u},
    {0x8B5Au, 4u},  {0x8B5Bu, 9u},  {0x8B5Cu, 16u},
    {0x8B65u, 6u},  {0x8B66u, 8u},  {0x8B67u, 6u},  {0x8B68u, 12u}, {0x8B69u, 8u},  {0x8B6Au, 12u},
    {0x8DC6u, 2u},  {0x8DC7u, 3u},  {0x8DC8u, 4u},
    {0x8FFCu, 2u},  {0x8FFDu, 3u},  {0x8FFEu, 4u},
    {0x8F46u, 4u},  {0x8F47u, 9u},  {0x8F48u, 16u},
    {0x8F49u, 6u},  {0x8F4Au, 8u},  {0x8F4Bu, 6u},  {0x8F4Cu, 12u}, {0x8F4Du, 8u},  {0x8F4Eu, 12u},
};

struct TypeRange {
    uint32_t first_;
    uint32_t last_;
};

constexpr TypeRange scalar_ranges[] = {
    {0x8B5Du, 0x8B64u}, {0x8DC0u, 0x8DC5u}, {0x8DC9u, 0x8DD8u}, {0x900Cu, 0x900Fu},
    {0x904Cu, 0x906Cu}, {0x9108u, 0x910Du}, {0x9055u, 0x9055u},
};

}

UniformShape::UniformShape() : program_(0u), location_(-1), values_(0u)
{
    AV_SCOPE("UniformShape::UniformShape");
}

UniformShape::UniformShape(uint32_t program, int32_t location, uint32_t type)
    : program_(program), location_(location), values_(values_of(type))
{
    AV_SCOPE("UniformShape::UniformShape");
}

uint32_t UniformShape::program() const
{
    AV_SCOPE("UniformShape::program");
    return program_;
}
int32_t  UniformShape::location() const
{
    AV_SCOPE("UniformShape::location");
    return location_;
}
uint32_t UniformShape::values() const
{
    AV_SCOPE("UniformShape::values");
    return values_;
}

uint32_t UniformShape::values_of(uint32_t type) const
{
    AV_SCOPE("UniformShape::values_of");
    for (const TypeValues &shaped : shaped_types)
        if (shaped.type_ == type) return shaped.values_;
    for (const TypeRange &range : scalar_ranges)
        if (type >= range.first_ && type <= range.last_) return 1u;
    return 0u;
}

}
