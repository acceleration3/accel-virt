/* One device's DXGI entry points, at the revision the runtime asked for
 * (contracts/dxgi_seam.md). There is no DXGI export: the table arrives inside
 * CreateDevice, in one arm of DXGI_DDI_BASE_ARGS's union, and which arm is
 * live follows from the DDI interface version - so the revision is decided
 * before the first slot is written, because writing 22 slots into the struct a
 * narrower revision names would write past the runtime's memory.
 *
 * The callbacks are kept as the pointer the runtime gave, never as copies of
 * the rows: the runtime may move any of them whenever no thread is inside the
 * driver.
 *
 * The answers are here rather than on the device because they are the seam:
 * ten of them are a capability this adapter does not have, said in DXGI's
 * spelling, and the rest are the refusal a slot gives until 056 or 058 fills
 * it. A removed device answers its removal code from every one of them. */
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
    /* Only the WDDM 3.2 revision is filled: this driver declares one DDI
     * version, so a runtime asking for an older one is asking for a driver
     * this is not. */
    enum class Revision : uint8_t { none, wddm_3_2 };

    /* dxgiddi.h:798, DXGI1_6_1_DDI_BASE_FUNCTIONS. */
    static constexpr uint32_t wddm_3_2_slots = 22u;

    explicit DxgiBaseTable(UmdDevice &device);
    DxgiBaseTable(const DxgiBaseTable &) = delete;
    DxgiBaseTable &operator=(const DxgiBaseTable &) = delete;

    /* Fills the arm `interface_version` names and records the callbacks.
     * hr_invalid_arg for null callbacks or a null functions pointer;
     * hr_no_interface for a version this driver does not serve - in either
     * case nothing is written into the runtime's memory. */
    Hresult publish(DxgiDdiBaseArgsRecord *args, uint32_t interface_version);

    Revision revision() const;
    bool     published() const;
    /* The table the runtime gave; null until published. */
    const DxgiDdiBaseCallbacksRecord *callbacks() const;

    /* The slots this adapter genuinely does not have, each answering hr_ok
     * with its capability at zero and nothing written past the field named. */
    Hresult gamma_caps(DxgiDdiArgGetGammaControlCapsRecord *args);
    Hresult resource_priority(DxgiDdiArgRecord *args);
    Hresult overlay_caps(DxgiDdiArgGetMultiplaneOverlayCapsRecord *args);
    Hresult overlay_group_caps(DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord *args);
    Hresult present_duration(DxgiDdiArgCheckPresentDurationSupportRecord *args);
    Hresult overlay_color_space(DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord *args);
    Hresult trim_residency(DxgiDdiArgRecord *args);
    /* Offered resources are never discarded: their memory is the host's, and
     * the guest has nothing to gain by letting it go. The runtime offers when
     * an application calls OfferResources, and removed the device when this
     * answered E_NOTIMPL - explorer then made a new one, 200 times a second. */
    Hresult offer_resources(DxgiDdiArgRecord *args);
    Hresult reclaim_resources(DxgiDdiArgReclaimResourcesRecord *args);
    Hresult reclaim_resources1(DxgiDdiArgReclaimResources1Record *args);
    /* Every slot a later feature fills: hr_not_impl. */
    Hresult until_later(DxgiDdiArgRecord *args);

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    /* hr_ok for arguments there are; otherwise what the caller must answer
     * instead. The drivers that draw (D3d11Driver) publish a table of their
     * own over this one, which answers for a device they lost. */
    Hresult usable(const DxgiDdiArgRecord *args) const;

    UmdDevice                        &device_;
    Revision                          revision_;
    const DxgiDdiBaseCallbacksRecord *callbacks_;

    friend struct DxgiBaseTableProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_DXGI_BASE_TABLE_HPP
