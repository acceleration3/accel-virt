#include "proto/d3d/dxgi_resolve_format.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

struct Family {
    uint32_t typeless;
    uint32_t unorm;
};

/* DXGI_FORMAT values (dxgiformat.h). */
constexpr Family colour_families[] = {
    {23u, 24u}, /* R10G10B10A2 */
    {27u, 28u}, /* R8G8B8A8 */
    {90u, 87u}, /* B8G8R8A8 */
    {92u, 88u}, /* B8G8R8X8 */
};

constexpr uint32_t other_typeless[] = {1u,  5u,  9u,  15u, 19u, 21u, 22u, 33u, 39u, 44u, 46u,
                                       47u, 48u, 53u, 60u, 70u, 73u, 76u, 79u, 82u, 94u, 97u};

}  // namespace

DxgiResolveFormat::DxgiResolveFormat(uint32_t format) : format_(format)
{
    AV_SCOPE("DxgiResolveFormat::DxgiResolveFormat");
}

uint32_t DxgiResolveFormat::typed() const
{
    AV_SCOPE("DxgiResolveFormat::typed");
    for (const Family &family : colour_families)
        if (family.typeless == format_) return family.unorm;
    for (const uint32_t typeless : other_typeless)
        if (typeless == format_) return 0u;
    return format_;
}

}  // namespace av
