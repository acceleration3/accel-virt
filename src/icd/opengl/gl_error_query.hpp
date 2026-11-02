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

    uint32_t ask();

private:
    ClientTransport &transport_;
};

}

#endif
