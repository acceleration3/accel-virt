#ifndef AV_D3D_HRESULT_HPP
#define AV_D3D_HRESULT_HPP

#include <stdint.h>

namespace av::d3d {

using Hresult = int32_t;

inline constexpr Hresult hr_ok = 0;
inline constexpr Hresult hr_not_impl = static_cast<Hresult>(0x80004001u);
inline constexpr Hresult hr_no_interface = static_cast<Hresult>(0x80004002u);
inline constexpr Hresult hr_fail = static_cast<Hresult>(0x80004005u);
inline constexpr Hresult hr_out_of_memory = static_cast<Hresult>(0x8007000Eu);
inline constexpr Hresult hr_invalid_arg = static_cast<Hresult>(0x80070057u);

inline constexpr Hresult hr_dxgi_invalid_call = static_cast<Hresult>(0x887A0001u);
inline constexpr Hresult hr_dxgi_not_found = static_cast<Hresult>(0x887A0002u);
inline constexpr Hresult hr_dxgi_unsupported = static_cast<Hresult>(0x887A0004u);
inline constexpr Hresult hr_dxgi_device_removed = static_cast<Hresult>(0x887A0005u);
inline constexpr Hresult hr_dxgi_device_hung = static_cast<Hresult>(0x887A0006u);
inline constexpr Hresult hr_dxgi_device_reset = static_cast<Hresult>(0x887A0007u);

inline constexpr Hresult hr_d3d12_driver_version_mismatch = static_cast<Hresult>(0x887E0002u);

}

#endif
