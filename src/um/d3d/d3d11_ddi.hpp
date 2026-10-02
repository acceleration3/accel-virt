/* The Direct3D 10/11 user-mode DDI as the WDK declares it, for the translation
 * units that implement the device table against it. The kit's user-mode
 * headers define no NTSTATUS, and d3dkmddi.h - which d3d10umddi.h includes -
 * is written in it, as src/um/kmt/gdi_kmt.cpp does for d3dkmthk.h. */
#ifndef AV_UM_D3D11_DDI_HPP
#define AV_UM_D3D11_DDI_HPP

#include <windows.h>

typedef _Return_type_success_(return >= 0) LONG NTSTATUS;

#include <d3d10umddi.h>
#include <dxgiddi.h>

#endif  // AV_UM_D3D11_DDI_HPP
