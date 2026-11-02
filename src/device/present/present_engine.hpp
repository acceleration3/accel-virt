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
    [[nodiscard]] Status attach(uint64_t address, const PresentFrames &snapshots);
    void                 detach();
    uint32_t             queue(ControlRequest &request);
    void                 run();

    bool invariants_hold() const;

private:
    QueuedPresent &slot(uint32_t index);
    void           release_moved();
    uint32_t       take(ControlRequest &request, uint64_t ticket);
    bool           refuse();
    QueuedPresent *next();
    void           complete(QueuedPresent &present, Status moved);
    void           stop_thread();

    Host                        &host_;
    FenceInterrupts             &interrupts_;
    void                        *lock_;
    void                        *wake_;
    void                        *thread_;
    bool                         stopping_;
    PresentPageWindow            page_;
    PresentFrames             snapshots_;
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
