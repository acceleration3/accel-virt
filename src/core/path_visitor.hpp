/* What VidPn::for_each_path calls with each present path of a topology. */
#ifndef AV_PATH_VISITOR_HPP
#define AV_PATH_VISITOR_HPP

#include "status.hpp"

namespace av {

struct D3dkmdtVidPnPresentPathRecord;

class PathVisitor {
public:
    /* A failure ends the walk and is what the walk answers. */
    [[nodiscard]] virtual Status visit(const D3dkmdtVidPnPresentPathRecord &path) = 0;

protected:
    PathVisitor() = default;
    ~PathVisitor() = default;
    PathVisitor(const PathVisitor &) = delete;
    PathVisitor &operator=(const PathVisitor &) = delete;
};

}  // namespace av

#endif  // AV_PATH_VISITOR_HPP
