/* What an application handed a clGet*Info to answer into: the room it gave,
 * where the answer goes, and where its size is said. Too little room is an
 * error that writes nothing, and no place for the answer asks its size alone. */
#ifndef AV_CLICD_CL_INFO_REQUEST_HPP
#define AV_CLICD_CL_INFO_REQUEST_HPP

#include <CL/cl.h>

#include <stddef.h>

namespace av::clicd {

class ClInfoRequest {
public:
    ClInfoRequest(size_t value_bytes, void *value, size_t *size_ret);
    ClInfoRequest(const ClInfoRequest &) = delete;
    ClInfoRequest &operator=(const ClInfoRequest &) = delete;

    [[nodiscard]] cl_int give(const void *from, size_t bytes) const;

private:
    size_t  value_bytes_;
    void   *value_;
    size_t *size_ret_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_INFO_REQUEST_HPP
