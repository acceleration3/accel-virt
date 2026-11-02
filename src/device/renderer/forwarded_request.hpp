#ifndef AV_FORWARDED_REQUEST_HPP
#define AV_FORWARDED_REQUEST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ForwardedRequest {
public:
    ForwardedRequest(const uint8_t *bytes, size_t length);
    ForwardedRequest(const ForwardedRequest &) = delete;
    ForwardedRequest &operator=(const ForwardedRequest &) = delete;

    size_t read(size_t offset, uint8_t *buffer, size_t size) const;

private:
    const uint8_t *bytes_;
    size_t         length_;
};

}

#endif
