#ifndef AV_SCRIPTED_SLICE_PROGRESS_HPP
#define AV_SCRIPTED_SLICE_PROGRESS_HPP

#include "executor/slice_progress.hpp"

#include <cstdint>

namespace av {

class ScriptedSliceProgress final : public SliceProgress {
public:
    ScriptedSliceProgress();
    ScriptedSliceProgress(std::uint64_t next, std::uint64_t reached);

    std::uint64_t next() const override;
    std::uint64_t reached() const override;

    void set(std::uint64_t next, std::uint64_t reached);
    void submit();
    void complete_through(std::uint64_t mark);

private:
    std::uint64_t next_;
    std::uint64_t reached_;
};

}

#endif
