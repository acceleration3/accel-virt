#ifndef AV_SLICE_PROGRESS_HPP
#define AV_SLICE_PROGRESS_HPP

#include <stdint.h>

namespace av {

class SliceProgress {
public:
    SliceProgress(const SliceProgress &) = delete;
    SliceProgress &operator=(const SliceProgress &) = delete;

    virtual uint64_t next() const = 0;
    virtual uint64_t reached() const = 0;

protected:
    SliceProgress() = default;
    ~SliceProgress() = default;
};

}

#endif
