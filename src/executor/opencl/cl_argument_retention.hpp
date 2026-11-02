#ifndef AV_EXECUTOR_CL_ARGUMENT_RETENTION_HPP
#define AV_EXECUTOR_CL_ARGUMENT_RETENTION_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {

class ClArgumentRetention {
public:
    ClArgumentRetention();
    ClArgumentRetention(const ClArgumentRetention &) = delete;
    ClArgumentRetention &operator=(const ClArgumentRetention &) = delete;

    bool kept(cl_context context, cl_device_id device);

private:
    enum class Answer : uint8_t { unmeasured, kept, lost };

    static constexpr const char *source = "__kernel void kept(__global int *argument) { argument[0] = 0; }\n";

    Answer answer_;
};

}

#endif
