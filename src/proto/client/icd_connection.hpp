/* What the generated Vulkan ICD wrappers ask of a connection (063
 * contracts/generated_code.md §F): the class, whose transport() a wrapper sends
 * through, and the two hooks vkCreateInstance and vkDestroyInstance call. The
 * hooks are defined in src/icd/vulkan/icd_hooks.cpp, each one line into the
 * bound IcdRoot. */
#ifndef AV_PROTO_ICD_CONNECTION_HPP
#define AV_PROTO_ICD_CONNECTION_HPP

#include "icd/vulkan/connection.hpp"

namespace av::icd {

/* The connection vkCreateInstance opened for the instance about to be made;
 * null when there is none, which the wrapper answers as
 * VK_ERROR_INITIALIZATION_FAILED. */
Connection *acquire_default_connection();
/* A failed vkCreateInstance's connection, or a destroyed instance's. */
void        release_connection(Connection *connection);

}  // namespace av::icd

#endif  // AV_PROTO_ICD_CONNECTION_HPP
