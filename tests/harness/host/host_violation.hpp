/* A rule of the host contract that MockHost caught being broken. */
#ifndef AV_HOST_VIOLATION_HPP
#define AV_HOST_VIOLATION_HPP

namespace av {

enum class HostViolation : unsigned {
    main_loop_required,   /* a main-loop method called from a worker */
    worker_required,      /* worker work run on the main loop */
    unknown_handle,       /* a thread, event or mutex the host never made, or already destroyed */
    held_mutex,           /* a mutex destroyed while locked, or unlocked while not */
    second_response,      /* a control request answered twice */
    overlapping_placement,/* a host memory block placed over another in the hostmem region */
    count
};

}  // namespace av

#endif  // AV_HOST_VIOLATION_HPP
