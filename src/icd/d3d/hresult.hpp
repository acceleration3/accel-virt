/* The HRESULT values the Direct3D user-mode driver DDI answers with, spelled
 * here because the portable core cannot include winerror.h. Every value is the
 * one the WDK's header defines (third_party/windows-kit/include/sdk-shared/
 * winerror.h); src/um/d3d/umd_record_layout.cpp asserts each against it, so a
 * transcription error fails the guest build rather than the guest. */
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

}  // namespace av::d3d

#endif  // AV_D3D_HRESULT_HPP
