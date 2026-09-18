/* A presented frame held until the adapter's next vertical blank on source 0.
 *
 * Vulkan's FIFO present mode is what asked for it (feature 046, research.md
 * §4); OpenGL's wglSwapIntervalEXT asks for the same thing, so it lives here
 * rather than in either ICD (feature 051, research.md §5). */
#ifndef AV_FRAME_PACER_HPP
#define AV_FRAME_PACER_HPP

#include <stdint.h>

namespace av {
class Kmt;
}  // namespace av

namespace av::icd {

class LogSink;
struct FramePacerProbe;

class FramePacer {
public:
    FramePacer(Kmt &kmt, LogSink &log, uint32_t adapter, uint32_t device);
    FramePacer(const FramePacer &) = delete;
    FramePacer &operator=(const FramePacer &) = delete;

    /* Waits for the next vertical blank. A failed wait is logged once and the
     * frame goes unpaced: a present has already put it on its window. */
    void     wait();
    uint64_t blanks() const;
    uint64_t failures() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t primary_source = 0u;

    Kmt     &kmt_;
    LogSink &log_;
    uint32_t adapter_;
    uint32_t device_;
    uint64_t blanks_;
    uint64_t failures_;

    friend struct FramePacerProbe;
};

}  // namespace av::icd

#endif  // AV_FRAME_PACER_HPP
