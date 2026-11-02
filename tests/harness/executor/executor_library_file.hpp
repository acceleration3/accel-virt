#ifndef AV_EXECUTOR_LIBRARY_FILE_HPP
#define AV_EXECUTOR_LIBRARY_FILE_HPP

#include "device/abi/av_executor.h"

namespace av {

class ExecutorLibraryFile {
public:
    explicit ExecutorLibraryFile(const char *path);
    ~ExecutorLibraryFile();
    ExecutorLibraryFile(const ExecutorLibraryFile &) = delete;
    ExecutorLibraryFile &operator=(const ExecutorLibraryFile &) = delete;

    const AvExecutorOps *ops() const;

private:
    void                 *library_;
    const AvExecutorOps *ops_;
};

}

#endif
