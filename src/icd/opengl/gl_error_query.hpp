/* glGetError on the host: the one command the generated client leaves to the
 * ICD (the profile's answered_by_guest, whose shared error word this device
 * does not publish), sent as the server decodes it - no arguments, the error
 * as a varint - on one context's transport. */
#ifndef AV_GLICD_GL_ERROR_QUERY_HPP
#define AV_GLICD_GL_ERROR_QUERY_HPP

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {

class GlErrorQuery {
public:
    explicit GlErrorQuery(ClientTransport &transport);

    /* The host's error; GL_NO_ERROR when the call or its answer failed. */
    uint32_t ask();

private:
    ClientTransport &transport_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_ERROR_QUERY_HPP
