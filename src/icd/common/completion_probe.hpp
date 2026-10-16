/* One guest wait's question to the host, asked once a slice (HostCompletionWait):
 * a Vulkan fence or semaphore wait, an OpenCL event list, an OpenGL sync. The
 * probe keeps its own API's answer; what HostCompletionWait learns is only
 * whether the wait is over. */
#ifndef AV_ICD_COMPLETION_PROBE_HPP
#define AV_ICD_COMPLETION_PROBE_HPP

#include <stdint.h>

namespace av::icd {

class CompletionProbe {
public:
    CompletionProbe(const CompletionProbe &) = delete;
    CompletionProbe &operator=(const CompletionProbe &) = delete;

    /* Asks the host once, letting it block for at most `slice` nanoseconds:
     * true when the wait is over - done, or failed - with the answer kept. */
    [[nodiscard]] virtual bool ask(uint64_t slice) = 0;
    /* Between two asks of a wait still pending: nothing where the host
     * blocked for the slice itself, a spin or a sleep where it answered at
     * once. */
    virtual void pause(uint64_t slice) = 0;

protected:
    CompletionProbe() = default;
    ~CompletionProbe() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_COMPLETION_PROBE_HPP
