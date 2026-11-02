#ifndef AV_PROTO_ICD_CONNECTION_HPP
#define AV_PROTO_ICD_CONNECTION_HPP

#include "icd/vulkan/connection.hpp"

namespace av::icd {
Connection *acquire_default_connection();
void        release_connection(Connection *connection);

}

#endif
