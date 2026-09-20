/* The 22 DXGI DDI entry points a WDDM 3.2 device publishes
 * (contracts/dxgi_seam.md). These are the ABI boundary C requires: the runtime
 * calls a bare function pointer with the DXGI argument structure only, so each
 * one is a single line - find the device behind the argument's hDevice, ask
 * it, return - and no logic lives here.
 *
 * On x64 Windows __stdcall is the only calling convention there is, so a table
 * of these portable pointers is the ABI the runtime calls; that is what lets
 * the whole seam be driven from Linux. It stops being true at 32 bits, which
 * is feature 061's (plan.md). */
#ifndef AV_D3D_DXGI_ENTRY_POINTS_HPP
#define AV_D3D_DXGI_ENTRY_POINTS_HPP

#include "icd/d3d/hresult.hpp"

namespace av::d3d {

struct Dxgi161DdiBaseFunctionsRecord;

/* Writes every slot of `table`. */
void fill_dxgi_base_functions(Dxgi161DdiBaseFunctionsRecord &table);

}  // namespace av::d3d

#endif  // AV_D3D_DXGI_ENTRY_POINTS_HPP
