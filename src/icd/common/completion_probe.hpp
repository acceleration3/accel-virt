#ifndef AV_ICD_COMPLETION_PROBE_HPP
#define AV_ICD_COMPLETION_PROBE_HPP

#include <stdint.h>

namespace av::icd {

class CompletionProbe {
public:
    CompletionProbe(const CompletionProbe &) = delete;
    CompletionProbe &operator=(const CompletionProbe &) = delete;

    [[nodiscard]] virtual bool ask(uint64_t slice) = 0;
    virtual void pause(uint64_t slice) = 0;

protected:
    CompletionProbe() = default;
    ~CompletionProbe() = default;
};

}

#endif
