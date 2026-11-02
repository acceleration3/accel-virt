#ifndef AV_D3D_UMD_ADAPTER_HPP
#define AV_D3D_UMD_ADAPTER_HPP

#include "icd/d3d/ddi_version_list.hpp"
#include "icd/d3d/runtime_kind.hpp"

#include <stdint.h>

namespace av {
class StateWriter;
}

namespace av::d3d {

struct D3dddiAdapterCallbacksRecord;
class UmdConnection;

struct UmdAdapterProbe;

class UmdAdapter {
public:
    UmdAdapter(RuntimeKind runtime, void *runtime_adapter, const D3dddiAdapterCallbacksRecord *callbacks,
               UmdConnection &connection, uint32_t interface_version, uint32_t runtime_version);
    UmdAdapter(const UmdAdapter &) = delete;
    UmdAdapter &operator=(const UmdAdapter &) = delete;

    RuntimeKind runtime() const;
    void       *runtime_adapter() const;
    const D3dddiAdapterCallbacksRecord *callbacks() const;
    UmdConnection                      &connection() const;
    uint32_t                            interface_version() const;
    uint32_t                            runtime_version() const;
    const DdiVersionList               &versions() const;

    void     device_made();
    void     device_gone();
    uint32_t devices() const;

    void report(StateWriter &writer, uint32_t index) const;

    bool invariants_hold() const;

private:
    RuntimeKind                         runtime_;
    void                               *runtime_adapter_;
    const D3dddiAdapterCallbacksRecord *callbacks_;
    UmdConnection                      &connection_;
    uint32_t                            interface_version_;
    uint32_t                            runtime_version_;
    uint32_t                            devices_;
    DdiVersionList                      versions_;

    friend struct UmdAdapterProbe;
};

}

#endif
