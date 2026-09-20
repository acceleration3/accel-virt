/* Which Direct3D runtime opened the adapter. The two are one DLL and one core,
 * but they resolve different exports, hand back differently shaped tables and
 * are versioned apart, so everything that differs between them is decided from
 * this rather than from which entry point was called (contracts/umd_interface.md). */
#ifndef AV_D3D_RUNTIME_KIND_HPP
#define AV_D3D_RUNTIME_KIND_HPP

#include <stdint.h>

namespace av::d3d {

enum class RuntimeKind : uint8_t {
    d3d10_11,
    d3d12,
};

}  // namespace av::d3d

#endif  // AV_D3D_RUNTIME_KIND_HPP
