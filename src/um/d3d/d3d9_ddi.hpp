/* The Direct3D 9 user-mode DDI as the WDK declares it (d3dumddi.h), for the
 * translation units that implement the device table against it. d3dumddi.h
 * names the API's types (D3DPRIMITIVETYPE, D3DCAPS9) without including their
 * headers, as a Direct3D 9 driver includes them first. */
#ifndef AV_UM_D3D9_DDI_HPP
#define AV_UM_D3D9_DDI_HPP

#include <windows.h>

typedef _Return_type_success_(return >= 0) LONG NTSTATUS;

#include <d3d9types.h>
#include <d3d9caps.h>
#include <d3dumddi.h>

#endif  // AV_UM_D3D9_DDI_HPP
