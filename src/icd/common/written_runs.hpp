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

    [[nodiscard]] bool note(uint64_t offset, uint64_t bytes);
    uint32_t           count() const;
    Run                run(uint32_t index) const;
    void               clear();

    bool invariants_hold() const;

private:
    Run      runs_[max_runs];
    uint32_t count_;
};

}

#endif
