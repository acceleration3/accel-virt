#ifndef AV_PRESENT_ENGINE_HPP
#define AV_PRESENT_ENGINE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/present/present_page_window.hpp"
#include "device/present/present_frames.hpp"
#include "device/present/queued_present.hpp"

#include <stdint.h>

namespace av {

class ControlRequest;
class FenceInterrupts;
class GuestRamLendings;
class Host;
struct PresentEngineProbe;

class PresentEngine {
public:
    static constexpr uint32_t depth = 16u;

    PresentEngine(Host &host, FenceInterrupts &interrupts);
    ~PresentEngine();
    PresentEngine(const PresentEngine &) = delete;
    PresentEngine &operator=(const PresentEngine &) = delete;

    [[nodiscard]] Status initialize();
    [[nodiscard]] Status attach(uint64_t address, const PresentFrames &snapshots, GuestRamLendings &lendings);
    void                 detach();
    uint32_t             queue(ControlRequest &request);
    void                 hold_moves();
    void                 resume_moves();
    void                 run();

    bool invariants_hold() const;

private:
    QueuedPresent &slot(uint32_t index);
    void           release_moved();
    uint32_t       take(ControlRequest &request, uint64_t ticket);
    bool           refuse();
    QueuedPresent *next();
    void           note_fallback(QueuedPresent::Fallback fallback);
    void           complete(QueuedPresent &present, Status moved);
    void           stop_thread();
    void           unregister();

    Host                        &host_;
    FenceInterrupts             &interrupts_;
    void                        *lock_;
    void                        *wake_;
    void                        *idle_;
    void                        *thread_;
    bool                         stopping_;
    bool                         holding_;
    bool                         moving_;
    PresentPageWindow            page_;
    PresentFrames             snapshots_;
    GuestRamLendings         *lendings_;
    bool                         told_[static_cast<uint32_t>(QueuedPresent::Fallback::count)];
    ObjectStorage<QueuedPresent> slots_[depth];
    uint32_t                     first_;
    uint32_t                     moved_;
    uint32_t                     held_;
    uint64_t                     counted_;
    uint64_t                     completed_;
    uint64_t                     refused_;

    friend struct PresentEngineProbe;
};

}

#endif
