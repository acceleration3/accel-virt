/* The functions that fill D3D12Core's four device tables, defined in the
 * generated thunks' translation unit (generated/d3d12_thunks.cpp, written by
 * tools/gen_d3d12_ddi.py): the ABI boundary C requires, one slot each. */
#ifndef AV_UM_D3D12_TABLES_HPP
#define AV_UM_D3D12_TABLES_HPP

#include "um/d3d/d3d12_ddi.hpp"

namespace av::d3d12 {

void fill_device_funcs12(D3D12DDI_DEVICE_FUNCS_CORE_0109 &table);
void fill_queue_funcs12(D3D12DDI_COMMAND_QUEUE_FUNCS_CORE_0001 &table);
void fill_list_funcs12(D3D12DDI_COMMAND_LIST_FUNCS_3D_0108 &table);
void fill_extended_funcs12(D3D12DDI_EXTENDED_FEATURES_FUNCS_0096 &table);

}  // namespace av::d3d12

#endif  // AV_UM_D3D12_TABLES_HPP
