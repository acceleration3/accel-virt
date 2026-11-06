#ifndef AV_CL_RUNNING_RIG_HPP
#define AV_CL_RUNNING_RIG_HPP

#include "harness/icd/cl_icd_rig.hpp"

#include <CL/cl.h>

#include <cstdint>

namespace av {

class ClRunningRig {
public:
    static constexpr std::uint64_t host_platform = 0x6200u;
    static constexpr std::uint64_t host_device = 0x6300u;
    static constexpr std::uint64_t host_context = 0x6400u;
    static constexpr std::uint64_t host_queue = 0x6500u;
    static constexpr std::uint64_t host_buffer = 0x6600u;
    static constexpr std::uint64_t host_program = 0x6700u;
    static constexpr std::uint64_t host_kernel = 0x6800u;
    static constexpr std::uint64_t host_event = 0x6900u;
    static constexpr std::uint64_t host_gate = 0x6950u;
    static constexpr std::uint64_t host_image = 0x6A00u;

    explicit ClRunningRig(ClIcdRig &rig);
    ClRunningRig(const ClRunningRig &) = delete;
    ClRunningRig &operator=(const ClRunningRig &) = delete;

    bool             made() const;
    ClIcdRig        &rig();
    cl_device_id     device() const;
    cl_context       context() const;
    cl_command_queue queue() const;
    cl_mem           buffer() const;
    cl_program       program() const;
    cl_kernel        kernel() const;
    cl_event         gate() const;

    cl_mem   image(std::uint64_t handle = host_image);
    cl_event queue_event();
    void     lose();

private:
    ClIcdRig        &rig_;
    cl_device_id     device_ = nullptr;
    cl_context       context_ = nullptr;
    cl_command_queue queue_ = nullptr;
    cl_mem           buffer_ = nullptr;
    cl_program       program_ = nullptr;
    cl_kernel        kernel_ = nullptr;
    cl_event         gate_ = nullptr;
};

}

#endif
