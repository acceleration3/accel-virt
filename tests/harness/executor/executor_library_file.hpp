/* The executor library as QEMU loads it (feature 045, T038): the shared object
 * at a path, opened with its own symbols kept to itself, and the table its
 * rvk_executor_ops answers. A test that also links the Vulkan ICD needs this
 * rather than the entry point in process: the ICD and the executor each define
 * the generated handle hooks their own way, and only the library's hidden
 * visibility keeps the two apart. Closed when this goes, but never unmapped:
 * a leak report after the test names its frames only while the code is there. */
#ifndef AV_EXECUTOR_LIBRARY_FILE_HPP
#define AV_EXECUTOR_LIBRARY_FILE_HPP

#include "device/abi/rvk_executor.h"

namespace av {

class ExecutorLibraryFile {
public:
    explicit ExecutorLibraryFile(const char *path);
    ~ExecutorLibraryFile();
    ExecutorLibraryFile(const ExecutorLibraryFile &) = delete;
    ExecutorLibraryFile &operator=(const ExecutorLibraryFile &) = delete;

    /* Null when the library did not open or answered no table. */
    const RvkExecutorOps *ops() const;

private:
    void                 *library_;
    const RvkExecutorOps *ops_;
};

}  // namespace av

#endif  // AV_EXECUTOR_LIBRARY_FILE_HPP
