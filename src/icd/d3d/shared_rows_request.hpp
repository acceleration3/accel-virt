#ifndef AV_D3D_SHARED_ROWS_REQUEST_HPP
#define AV_D3D_SHARED_ROWS_REQUEST_HPP

#include <stdint.h>

namespace av::d3d {

class SharedRowsRequest {
public:
    static constexpr uint32_t most_threads = 8u;

    SharedRowsRequest();
    SharedRowsRequest(const SharedRowsRequest &) = delete;
    SharedRowsRequest &operator=(const SharedRowsRequest &) = delete;

    bool ask(uint32_t thread);
    void withdraw(uint32_t thread);
    bool asked_by(uint32_t thread) const;

private:
    uint32_t slot_of(uint32_t thread) const;

    uint32_t threads_[most_threads];
};

}

#endif
