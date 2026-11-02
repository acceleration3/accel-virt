#ifndef AV_D3D_DXGI_DDI_BASE_ARGS_RECORD_HPP
#define AV_D3D_DXGI_DDI_BASE_ARGS_RECORD_HPP

namespace av::d3d {

struct DxgiDdiBaseArgsRecord {
    void *pDXGIBaseCallbacks;
    void *pDXGIDDIBaseFunctions;
};

}

#endif
