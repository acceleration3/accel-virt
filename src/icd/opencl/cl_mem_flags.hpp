#ifndef AV_CLICD_CL_MEM_FLAGS_HPP
#define AV_CLICD_CL_MEM_FLAGS_HPP

#include <CL/cl.h>

namespace av::clicd {

class ClMemFlags {
public:
    explicit ClMemFlags(cl_mem_flags flags);

    [[nodiscard]] cl_int judge() const;
    cl_mem_flags         for_host() const;
    cl_mem_flags         asked() const;
    bool                 lets_host_read() const;
    bool                 lets_host_write() const;

private:
    static constexpr cl_mem_flags host_access = CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;
    static constexpr cl_mem_flags device_access = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY;

    static unsigned bits_of(cl_mem_flags flags);

    cl_mem_flags flags_;
};

}

#endif
