#ifndef AV_ENGINE_PROGRESS_HPP
#define AV_ENGINE_PROGRESS_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class ExecutionNode;

class EngineProgress {
public:
    EngineProgress(Platform &platform, ExecutionNode &node);
    EngineProgress(const EngineProgress &) = delete;
    EngineProgress &operator=(const EngineProgress &) = delete;

    [[nodiscard]] bool responsive();

private:
    Platform      &platform_;
    ExecutionNode &node_;
    uint64_t       answered_progress_;
    bool           answered_;
};

}

#endif
