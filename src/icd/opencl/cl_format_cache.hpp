#ifndef AV_CLICD_CL_FORMAT_CACHE_HPP
#define AV_CLICD_CL_FORMAT_CACHE_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av::clicd {

class ClFormatCache {
public:
    static constexpr uint32_t most_entries = 12u;
    static constexpr uint32_t most_formats = 160u;

    ClFormatCache();
    ClFormatCache(const ClFormatCache &) = delete;
    ClFormatCache &operator=(const ClFormatCache &) = delete;

    const cl_image_format *find(cl_mem_flags flags, cl_mem_object_type type, uint32_t &count) const;
    bool                   keep(cl_mem_flags flags, cl_mem_object_type type, const cl_image_format *formats,
                                uint32_t count);

private:
    struct Entry {
        cl_mem_flags       flags_;
        cl_mem_object_type type_;
        uint32_t           count_;
        bool               used_;
        cl_image_format    formats_[most_formats];
    };

    Entry entries_[most_entries];
};

}

#endif
