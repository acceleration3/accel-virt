/* D3DDDI_ADAPTERCALLBACKS (d3dumddi.h), the table both OpenAdapter entry points
 * are handed. A DDI record: the runtime owns this memory and may move the
 * functions whenever no thread is inside the driver, so the driver keeps the
 * pointer to the table and never a copy of a row. Offsets proven in
 * src/um/d3d/umd_record_layout.cpp. */
#ifndef AV_D3D_D3DDDI_ADAPTER_CALLBACKS_RECORD_HPP
#define AV_D3D_D3DDDI_ADAPTER_CALLBACKS_RECORD_HPP

namespace av::d3d {

struct D3dddiAdapterCallbacksRecord {
    void *pfnQueryAdapterInfoCb;
    void *pfnGetMultisampleMethodListCb;
    void *pfnQueryAdapterInfoCb2;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3DDDI_ADAPTER_CALLBACKS_RECORD_HPP
