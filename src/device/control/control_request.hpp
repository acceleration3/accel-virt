#ifndef AV_CONTROL_REQUEST_HPP
#define AV_CONTROL_REQUEST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class GpuResponse;
class Host;

class ControlRequest {
public:
    ControlRequest(Host &host, void *request);
    ControlRequest(const ControlRequest &) = delete;
    ControlRequest &operator=(const ControlRequest &) = delete;

    size_t read(size_t offset, uint8_t *buffer, size_t size);
    void   respond(const GpuResponse &response);
    bool   responded() const;

private:
    Host &host_;
    void *request_;
    bool  responded_;
};

}

#endif
