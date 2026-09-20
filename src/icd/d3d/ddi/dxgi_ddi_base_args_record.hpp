/* DXGI_DDI_BASE_ARGS (dxgiddi.h:1013), the DXGI half of CreateDevice. The
 * union of seven function-table pointers is one pointer: which revision it
 * points at is the DDI interface version the runtime asked for, not something
 * read out of the structure (contracts/dxgi_seam.md). A DDI record. */
#ifndef AV_D3D_DXGI_DDI_BASE_ARGS_RECORD_HPP
#define AV_D3D_DXGI_DDI_BASE_ARGS_RECORD_HPP

namespace av::d3d {

struct DxgiDdiBaseArgsRecord {
    void *pDXGIBaseCallbacks;
    void *pDXGIDDIBaseFunctions;
};

}  // namespace av::d3d

#endif  // AV_D3D_DXGI_DDI_BASE_ARGS_RECORD_HPP
