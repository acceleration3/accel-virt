/* The API a context speaks. Values are the capset's sub_apis bits. */
#ifndef AV_SUB_API_HPP
#define AV_SUB_API_HPP

#include <stdint.h>

namespace av {

enum class SubApi : uint32_t { vulkan = 1u, opengl = 2u, opencl = 4u, d3d = 8u };

}  // namespace av

#endif  // AV_SUB_API_HPP
