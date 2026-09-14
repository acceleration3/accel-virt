/* Node 0's submission fences as VidSch keeps them for the processes waiting on
 * them: the next fence a submission gets, which context each went to, and
 * what the driver's notifications made of them. DMA_COMPLETED finishes every
 * fence up to the one it names, since the node completes in order;
 * DMA_FAULTED names a fence, and the context that submitted it is lost -
 * nothing it submitted will complete, and a wait on it fails. Contexts are
 * the caller's slot numbers. */
#ifndef AV_NODE_FENCES_HPP
#define AV_NODE_FENCES_HPP

#include "core/ddi/dxgkargcb_notify_interrupt_data_record.hpp"
#include "core/support/status.hpp"

#include <cstdint>

namespace av {

class NodeFences {
public:
    static constexpr std::uint32_t max_contexts = 16u;
    /* Submissions remembered for naming a faulted fence's context: more than
     * the driver's node holds pending. */
    static constexpr std::uint32_t history = 256u;

    NodeFences();

    /* The fence the next submission is given. */
    std::uint32_t next() const;
    /* The submission on `context` was taken with next()'s fence. */
    void          commit(std::uint32_t context);
    void          observe(const DxgkargcbNotifyInterruptDataRecord &data);
    /* ok once everything `context` submitted has completed, invalid_state
     * once it is lost, not_found while something is still running. */
    Status        settled(std::uint32_t context) const;
    bool          lost(std::uint32_t context) const;
    /* A context slot made free, so the next one in it starts clean. */
    void          forget(std::uint32_t context);
    std::uint32_t completed() const;
    std::uint32_t faults() const;
    /* DXGK_INTERRUPT_CRTC_VSYNC notifications seen. */
    std::uint32_t vertical_blanks() const;

private:
    enum : std::int32_t { dma_completed = 1, crtc_vsync = 3, dma_faulted = 4 };
    static constexpr std::uint32_t no_context = max_contexts;

    struct Submission {
        std::uint32_t fence;
        std::uint32_t context;
    };

    void lose_submitter_of(std::uint32_t fence);

    std::uint32_t next_;
    std::uint32_t completed_;
    std::uint32_t faults_;
    std::uint32_t blanks_;
    std::uint32_t last_[max_contexts];
    bool          lost_[max_contexts];
    Submission    submissions_[history];
    std::uint32_t recorded_;
};

}  // namespace av

#endif  // AV_NODE_FENCES_HPP
