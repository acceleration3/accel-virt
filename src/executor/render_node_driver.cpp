#include "executor/render_node_driver.hpp"

#include "core/trace/function_scope.hpp"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace av {

RenderNodeDriver::RenderNodeDriver(const char *class_directory) : class_directory_{}
{
    AV_SCOPE("RenderNodeDriver::RenderNodeDriver");
    snprintf(class_directory_, sizeof class_directory_, "%s", class_directory != nullptr ? class_directory : "");
}

bool RenderNodeDriver::named(int32_t render_minor, const char *driver) const
{
    AV_SCOPE("RenderNodeDriver::named");
    if (render_minor < 0 || driver == nullptr) return false;
    char link[256];
    snprintf(link, sizeof link, "%s/renderD%d/device/driver", class_directory_, render_minor);
    char         target[256] = {};
    const ssize_t length = readlink(link, target, sizeof target - 1u);
    if (length <= 0) return false;
    const char *const last = strrchr(target, '/');
    return strcmp(last != nullptr ? last + 1 : target, driver) == 0;
}

}
