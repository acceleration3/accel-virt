#ifndef AV_D3D_DXGI_BASE_TABLE_HPP
#define AV_D3D_DXGI_BASE_TABLE_HPP

#include "icd/d3d/hresult.hpp"

#include <stdint.h>

namespace av {
class StateWriter;
}

namespace av::d3d {

class UmdDevice;
struct DxgiDdiArgRecord;
struct DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord;
struct DxgiDdiArgCheckPresentDurationSupportRecord;
struct DxgiDdiArgGetGammaControlCapsRecord;
struct DxgiDdiArgGetMultiplaneOverlayCapsRecord;
struct DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord;
struct DxgiDdiArgReclaimResourcesRecord;
struct DxgiDdiArgReclaimResources1Record;
struct DxgiDdiBaseArgsRecord;
struct DxgiDdiBaseCallbacksRecord;

struct DxgiBaseTableProbe;

class DxgiBaseTable {
public:
    enum class Revision : uint8_t { none, wddm_3_2 };

    static constexpr uint32_t wddm_3_2_slots = 22u;

    explicit DxgiBaseTable(UmdDevice &device);
    DxgiBaseTable(const DxgiBaseTable &) = delete;
    DxgiBaseTable &operator=(const DxgiBaseTable &) = delete;

    Hresult publish(DxgiDdiBaseArgsRecord *args, uint32_t interface_version);

    Revision revision() const;
    bool     published() const;
    const DxgiDdiBaseCallbacksRecord *callbacks() const;

    Hresult gamma_caps(DxgiDdiArgGetGammaControlCapsRecord *args);
    Hresult resource_priority(DxgiDdiArgRecord *args);
    Hresult overlay_caps(DxgiDdiArgGetMultiplaneOverlayCapsRecord *args);
    Hresult overlay_group_caps(DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord *args);
    Hresult present_duration(DxgiDdiArgCheckPresentDurationSupportRecord *args);
    Hresult overlay_color_space(DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord *args);
    Hresult trim_residency(DxgiDdiArgRecord *args);
    Hresult offer_resources(DxgiDdiArgRecord *args);
    Hresult reclaim_resources(DxgiDdiArgReclaimResourcesRecord *args);
    Hresult reclaim_resources1(DxgiDdiArgReclaimResources1Record *args);
    Hresult until_later(DxgiDdiArgRecord *args);

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    Hresult usable(const DxgiDdiArgRecord *args) const;

    UmdDevice                        &device_;
    Revision                          revision_;
    const DxgiDdiBaseCallbacksRecord *callbacks_;

    friend struct DxgiBaseTableProbe;
};

}

#endif
