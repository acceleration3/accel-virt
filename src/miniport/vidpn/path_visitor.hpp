#ifndef AV_PATH_VISITOR_HPP
#define AV_PATH_VISITOR_HPP

#include "core/support/status.hpp"

namespace av {

struct D3dkmdtVidPnPresentPathRecord;

class PathVisitor {
public:
    [[nodiscard]] virtual Status visit(const D3dkmdtVidPnPresentPathRecord &path) = 0;

protected:
    PathVisitor() = default;
    ~PathVisitor() = default;
    PathVisitor(const PathVisitor &) = delete;
    PathVisitor &operator=(const PathVisitor &) = delete;
};

}

#endif
