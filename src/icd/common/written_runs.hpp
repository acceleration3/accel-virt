/* The runs of a map's private copy to send back to the host: what the
 * application wrote, as each API finds it - OpenGL by comparing the copy
 * with what the host holds, Direct3D 11 by the pages the process's write
 * watch saw written - noted in order and merged.
 *
 * Runs closer than merge_gap go back as one: the bytes between are the
 * host's own, and a run costs a command. A write watch's runs are whole
 * pages, so only touching ones merge there. Past max_runs a note is refused:
 * Direct3D 11 then sends the whole copy, OpenGL sends what it holds and goes
 * on noting - a refused note leaves every run before it final, since nothing
 * noted later could merge into them. */
#ifndef AV_ICD_WRITTEN_RUNS_HPP
#define AV_ICD_WRITTEN_RUNS_HPP

#include <stdint.h>

namespace av::icd {

class WrittenRuns {
public:
    struct Run {
        uint64_t offset_;
        uint64_t bytes_;
    };
    static constexpr uint32_t max_runs = 32u;
    static constexpr uint64_t merge_gap = 256u;

    WrittenRuns();
    WrittenRuns(const WrittenRuns &) = delete;
    WrittenRuns &operator=(const WrittenRuns &) = delete;

    /* `bytes` written from `offset`, merged into the last run when fewer
     * than merge_gap bytes lie between them. False, with nothing noted, when
     * it would be a run past max_runs or starts before the last run ends -
     * runs come in order, and one out of it cannot be merged. Nothing is
     * noted of no bytes, or of a run that would end past the largest
     * offset. */
    [[nodiscard]] bool note(uint64_t offset, uint64_t bytes);
    uint32_t           count() const;
    /* The run at `index`, which is below count(). */
    Run                run(uint32_t index) const;
    void               clear();

    /* Runs are in order, none empty, and none closer than merge_gap. */
    bool invariants_hold() const;

private:
    Run      runs_[max_runs];
    uint32_t count_;
};

}  // namespace av::icd

#endif  // AV_ICD_WRITTEN_RUNS_HPP
