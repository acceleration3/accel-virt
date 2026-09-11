/* D3DKMDT_GDISURFACEDATA, a GDI surface standard allocation; the driver
 * answers Pitch. A DDI record: Flags is the D3DKMDT_GDISURFACEFLAGS Value.
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_D3DKMDT_GDI_SURFACE_DATA_RECORD_HPP
#define AV_D3DKMDT_GDI_SURFACE_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

struct D3dkmdtGdiSurfaceDataRecord {
    uint32_t Width;
    uint32_t Height;
    int32_t  Format;
    int32_t  Type;
    uint32_t Flags;
    uint32_t Pitch;
};

}  // namespace av

#endif  // AV_D3DKMDT_GDI_SURFACE_DATA_RECORD_HPP
