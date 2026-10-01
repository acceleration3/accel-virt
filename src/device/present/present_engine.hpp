/* The present engine (docs/protocol.md, "Present engine"): the presents the
 * guest queues are taken on the main loop in the order they arrive and moved
 * on a thread of the engine's own, one after another, off the main loop. Each
 * ticket completes in its turn in the guest's page - a refused one with the
 * present before it, or at once when none is outstanding - and the present
 * line is raised as they do. The engine runs from the page the guest
 * registers until a reset. */
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
    /* The presents taken and not yet moved, at most. */
    static constexpr uint32_t depth = 16u;

    PresentEngine(Host &host, FenceInterrupts &interrupts);
    ~PresentEngine();
    PresentEngine(const PresentEngine &) = delete;
    PresentEngine &operator=(const PresentEngine &) = delete;

    /* Main loop. no_memory when the host has no mutex or event; invalid_state
     * once done. */
    [[nodiscard]] Status initialize();
    /* Main loop. The guest's page from now on, counting from ticket 1, and
     * every present taken before forgotten: 0 lets the page go. Image sources
     * are read from `snapshots`. invalid_state before initialize; the page's
     * refusal (PresentPageWindow::map), which leaves no page; no_memory when
     * no thread starts. */
    [[nodiscard]] Status attach(uint64_t address, const PresentFrames &snapshots);
    /* Main loop. A virtio reset: the thread stopped, the page let go, and every
     * present not yet moved forgotten. */
    void                 detach();
    /* Main loop. QUEUED_PRESENT's response type: OK_NODATA once taken, an
     * error when refused - a refusal completes in its turn all the same, but
     * without a page there is no turn to take. */
    uint32_t             queue(ControlRequest &request);
    /* The thread's body, entered through RvkPresentEngineRun. */
    void                 run();

    bool invariants_hold() const;

private:
    QueuedPresent &slot(uint32_t index);
    /* Under the lock, main loop: the moved presents' mappings let go. */
    void           release_moved();
    /* Main loop: the response for the request counted as `ticket`. */
    uint32_t       take(ControlRequest &request, uint64_t ticket);
    /* Under the lock: the last ticket counted is refused. Whether it completed
     * now, with nothing outstanding for it to follow. */
    bool           refuse();
    /* The thread: the oldest present not moved, once there is one; null once
     * stopping. */
    QueuedPresent *next();
    /* The thread: `present` has moved, or failed to. */
    void           complete(QueuedPresent &present, Status moved);
    void           stop_thread();

    Host                        &host_;
    FenceInterrupts             &interrupts_;
    void                        *lock_;
    void                        *wake_;
    void                        *thread_;
    bool                         stopping_;
    PresentPageWindow            page_;
    /* Set while the thread is stopped, read by it after. */
    PresentFrames             snapshots_;
    ObjectStorage<QueuedPresent> slots_[depth];
    /* The ring: `held_` slots from `first_`, the first `moved_` of them moved
     * and waiting for the main loop to let them go. */
    uint32_t                     first_;
    uint32_t                     moved_;
    uint32_t                     held_;
    /* Tickets counted since the page, the last completed, and how many of
     * those were refused - what the page says. */
    uint64_t                     counted_;
    uint64_t                     completed_;
    uint64_t                     refused_;

    friend struct PresentEngineProbe;
};

}  // namespace av

#endif  // AV_PRESENT_ENGINE_HPP
