#ifndef AV_HOST_VIOLATION_HPP
#define AV_HOST_VIOLATION_HPP

namespace av {

enum class HostViolation : unsigned {
    main_loop_required,
    worker_required,
    unknown_handle,
    held_mutex,
    second_response,
    overlapping_placement,
    fence_misuse,
    doorbell_misuse,
    scanout_misuse,
    count
};

}

#endif
