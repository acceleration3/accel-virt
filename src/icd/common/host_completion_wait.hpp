#ifndef AV_ICD_HOST_COMPLETION_WAIT_HPP
#define AV_ICD_HOST_COMPLETION_WAIT_HPP

#include "icd/common/sliced_wait.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}

namespace av::icd {

class CompletionProbe;

class HostCompletionWait {
public:
    HostCompletionWait(const Kmt &kmt, uint64_t timeout);
    HostCompletionWait(const HostCompletionWait &) = delete;
    HostCompletionWait &operator=(const HostCompletionWait &) = delete;

    [[nodiscard]] bool run(CompletionProbe &probe);

private:
    const Kmt &kmt_;
    uint64_t   frequency_;
    uint64_t   start_;
    SlicedWait wait_;
};

}

#endif
