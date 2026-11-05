#ifndef AV_VIRTIO_GPU_AV_HPP
#define AV_VIRTIO_GPU_AV_HPP

#include "core/support/status.hpp"
#include "device/gpu/device_capset.hpp"
#include "device/context/local_context_plane.hpp"
#include "device/isolation/renderer_context_plane.hpp"
#include "device/display/display_clock.hpp"
#include "device/display/head_planes.hpp"
#include "device/display/display_heads.hpp"
#include "device/display/display_modes.hpp"
#include "device/export/export_service.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/module/driver_status.hpp"
#include "device/gpu/gpu_binding.hpp"
#include "device/gpu/host_pci_identity.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/memory/window_stats_window.hpp"
#include "device/present/present_engine.hpp"
#include "device/feasibility/host_feasibility.hpp"
#include "device/recording/wire_recorder.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class GuestRamLendings;
class Host;
class TextBuffer;
struct VirtioGpuAvProbe;

class VirtioGpuAv {
public:
    explicit VirtioGpuAv(Host &host);
    void share_lendings(GuestRamLendings &lendings);
    VirtioGpuAv(const VirtioGpuAv &) = delete;
    VirtioGpuAv &operator=(const VirtioGpuAv &) = delete;

    [[nodiscard]] Status realize(const char *host_gpu, bool base_blobs, const AvExecutorOps *executor,
                                 const AvRecordingOptions *recording, const char *renderer,
                                 const char *executor_path, TextBuffer &message);
    void                 reset();
    void                 status(uint8_t status);
    bool                 control(void *request, uint32_t type);
    [[nodiscard]] Status set_modes(const char *text, uint32_t &width, uint32_t &height);
    int                  offer_modes(const char *text, uint32_t *width, uint32_t *height);
    [[nodiscard]] Status set_refresh(uint32_t millihz);
    bool                 offer_refresh(uint32_t millihz);
    uint32_t             capsets() const;
    bool                 export_image(uint64_t key, AvExportedImage &image);
    bool                 export_head(uint32_t head, uint64_t key, AvExportedImage &image);
    bool                 read_image(uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height);
    bool                 post_frame(uint32_t head, uint32_t frame, uint64_t key);
    void                 show_latched_frames();
    bool                 pci_identity(AvPciIdentity &identity) const;
    uint32_t             blobs() const;
    uint32_t             scanouts() const;
    [[nodiscard]] Status create_context(uint32_t id, const char *name);
    [[nodiscard]] Status destroy_context(uint32_t id);
    uint32_t             contexts() const;
    void                 renderer_lost();
    bool                 export_accepted(int connection, bool same_user);
    bool                 realized() const;
    uint32_t             gpu_index() const;
    void                 report(StateWriter &writer);

    bool invariants_hold() const;

private:
    [[nodiscard]] Status start_recording(const AvRecordingOptions *recording, TextBuffer &message);
    void                 attach_exports();

    Host              &host_;
    GpuBinding         binding_;
    HostPciIdentity    pci_;
    HostFeasibility    feasibility_;
    DeviceCapset       capset_;
    WireRecorder       recorder_;
    BlobTable          blobs_;
    HostBlobTable      host_blobs_;
    WindowStatsWindow  window_stats_;
    DisplayHeads       heads_;
    DisplayModes       display_modes_;
    FenceInterrupts    interrupts_;
    DisplayClock       clock_;
    HeadPlanes         planes_;
    PresentEngine      presents_;
    DriverStatus       status_;
    LocalContextPlane    local_;
    RendererContextPlane remote_;
    ContextPlane        *contexts_;
    ExportService        exports_;

    friend struct VirtioGpuAvProbe;
};

}

#endif
